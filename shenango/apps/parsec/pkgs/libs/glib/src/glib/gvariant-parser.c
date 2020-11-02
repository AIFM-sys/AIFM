/*
 * Copyright © 2009, 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

#include "galias.h"

/*
 * two-pass algorithm
 * designed by ryan lortie and william hua
 * designed in itb-229 and at ghazi's, 2009.
 */

/**
 * G_VARIANT_PARSE_ERROR:
 *
 * Error domain for GVariant text format parsing.  Specific error codes
 * are not currently defined for this domain.  See #GError for
 * information on error domains.
 **/
GQuark
g_variant_parser_get_error_quark (void)
{
  static GQuark the_quark;

  if (the_quark == 0)
    the_quark = g_quark_from_static_string ("g-variant-parse-error-quark");

  return the_quark;
}

typedef struct
{
  gint start, end;
} SourceRef;

static void
parser_set_error_va (GError      **error,
                     SourceRef    *location,
                     SourceRef    *other,
                     const gchar  *format,
                     va_list       ap)
{
  GString *msg = g_string_new (NULL);

  if (location->start == location->end)
    g_string_append_printf (msg, "%d", location->start);
  else
    g_string_append_printf (msg, "%d-%d", location->start, location->end);

  if (other != NULL)
    {
      g_assert (other->start != other->end);
      g_string_append_printf (msg, ",%d-%d", other->start, other->end);
    }
  g_string_append_c (msg, ':');

  g_string_append_vprintf (msg, format, ap);
  g_set_error_literal (error, G_VARIANT_PARSE_ERROR, 0, msg->str);
  g_string_free (msg, TRUE);
}

static void
parser_set_error (GError      **error,
                  SourceRef    *location,
                  SourceRef    *other,
                  const gchar  *format,
                  ...)
{
  va_list ap;

  va_start (ap, format);
  parser_set_error_va (error, location, other, format, ap);
  va_end (ap);
}

typedef struct
{
  const gchar *start;
  const gchar *stream;
  const gchar *end;

  const gchar *this;
} TokenStream;


static void
token_stream_set_error (TokenStream  *stream,
                        GError      **error,
                        gboolean      this_token,
                        const gchar  *format,
                        ...)
{
  SourceRef ref;
  va_list ap;

  ref.start = stream->this - stream->start;

  if (this_token)
    ref.end = stream->stream - stream->start;
  else
    ref.end = ref.start;

  va_start (ap, format);
  parser_set_error_va (error, &ref, NULL, format, ap);
  va_end (ap);
}

static void
token_stream_prepare (TokenStream *stream)
{
  gint brackets = 0;
  const gchar *end;

  if (stream->this != NULL)
    return;

  while (stream->stream != stream->end && g_ascii_isspace (*stream->stream))
    stream->stream++;

  if (stream->stream == stream->end || *stream->stream == '\0')
    {
      stream->this = stream->stream;
      return;
    }

  switch (stream->stream[0])
    {
    case '-': case '+': case '.': case '0': case '1': case '2':
    case '3': case '4': case '5': case '6': case '7': case '8':
    case '9':
      for (end = stream->stream; end != stream->end; end++)
        if (!g_ascii_isalnum (*end) &&
            *end != '-' && *end != '+' && *end != '.')
          break;
      break;

    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z':
      for (end = stream->stream; end != stream->end; end++)
        if (!g_ascii_isalnum (*end))
          break;
      break;

    case '@': case '%':
      /* stop at the first space, comma, colon or unmatched bracket.
       * deals nicely with cases like (%i, %i) or {%i: %i}.
       */
      for (end = stream->stream + 1;
           end != stream->end && *end != ',' &&
           *end != ':' && !g_ascii_isspace (*end);
           end++)

        if (*end == '(' || *end == '{')
          brackets++;

        else if ((*end == ')' || *end == '}') && !brackets--)
          break;

      break;

    case '\'': case '"':
      for (end = stream->stream + 1; end != stream->end; end++)
        if (*end == stream->stream[0] || *end == '\0' ||
            (*end == '\\' && (++end == stream->end || *end == '\0')))
          break;

      if (end != stream->end && *end)
        end++;
      break;

    default:
      end = stream->stream + 1;
      break;
    }

  stream->this = stream->stream;
  stream->stream = end;
}

static void
token_stream_next (TokenStream *stream)
{
  stream->this = NULL;
}

static gboolean
token_stream_peek (TokenStream *stream,
                   gchar        first_char)
{
  token_stream_prepare (stream);

  return stream->this[0] == first_char;
}

static gboolean
token_stream_is_keyword (TokenStream *stream)
{
  token_stream_prepare (stream);

  return g_ascii_isalpha (stream->this[0]);
}

static gboolean
token_stream_is_numeric (TokenStream *stream)
{
  token_stream_prepare (stream);

  return (g_ascii_isdigit (stream->this[0]) ||
          stream->this[0] == '-' ||
          stream->this[0] == '+' ||
          stream->this[0] == '.');
}

static gboolean
token_stream_consume (TokenStream *stream,
                      const gchar *token)
{
  gint length = strlen (token);

  token_stream_prepare (stream);

  if (stream->stream - stream->this == length &&
      memcmp (stream->this, token, length) == 0)
    {
      token_stream_next (stream);
      return TRUE;
    }

  return FALSE;
}

static gboolean
token_stream_require (TokenStream  *stream,
                      const gchar  *token,
                      const gchar  *purpose,
                      GError      **error)
{

  if (!token_stream_consume (stream, token))
    {
      token_stream_set_error (stream, error, FALSE,
                              "expected `%s'%s", token, purpose);
      return FALSE;
    }

  return TRUE;
}

static void
token_stream_assert (TokenStream *stream,
                     const gchar *token)
{
  gboolean correct_token;

  correct_token = token_stream_consume (stream, token);
  g_assert (correct_token);
}

static gchar *
token_stream_get (TokenStream *stream)
{
  gchar *result;

  token_stream_prepare (stream);

  result = g_strndup (stream->this, stream->stream - stream->this);

  return result;
}

static void
token_stream_start_ref (TokenStream *stream,
                        SourceRef   *ref)
{
  token_stream_prepare (stream);
  ref->start = stream->this - stream->start;
}

static void
token_stream_end_ref (TokenStream *stream,
                      SourceRef   *ref)
{
  ref->end = stream->stream - stream->start;
}

void
pattern_copy (gchar       **out,
              const gchar **in)
{
  gint brackets = 0;

  while (**in == 'a' || **in == 'm' || **in == 'M')
    *(*out)++ = *(*in)++;

  do
    {
      if (**in == '(' || **in == '{')
        brackets++;

      else if (**in == ')' || **in == '}')
        brackets--;

      *(*out)++ = *(*in)++;
    }
  while (brackets);
}

static gchar *
pattern_coalesce (const gchar *left,
                  const gchar *right)
{
  gchar *result;
  gchar *out;

  /* the length of the output is loosely bound by the sum of the input
   * lengths, not simply the greater of the two lengths.
   *
   *   (*(iii)) + ((iii)*) ((iii)(iii))
   *
   *      8     +    8    =  12
   */
  out = result = g_malloc (strlen (left) + strlen (right));

  while (*left && *right)
    {
      if (*left == *right)
        {
          *out++ = *left++;
          right++;
        }

      else
        {
          const gchar **one = &left, **the_other = &right;

         again:
          if (**one == '*' && **the_other != ')')
            {
              pattern_copy (&out, the_other);
              (*one)++;
            }

          else if (**one == 'M' && **the_other == 'm')
            {
              *out++ = *(*the_other)++;
            }

          else if (**one == 'M' && **the_other != 'm')
            {
              (*one)++;
            }

          else if (**one == 'N' && strchr ("ynqiuxthd", **the_other))
            {
              *out++ = *(*the_other)++;
              (*one)++;
            }

          else if (**one == 'S' && strchr ("sog", **the_other))
            {
              *out++ = *(*the_other)++;
              (*one)++;
            }

          else if (one == &left)
            {
              one = &right, the_other = &left;
              goto again;
            }

          else
            break;
        }
    }

  if (*left || *right)
    {
      g_free (result);
      result = NULL;
    }
  else
    *out++ = '\0';

  return result;
}

typedef struct _AST AST;
typedef gchar *    (*get_pattern_func)    (AST                 *ast,
                                           GError             **error);
typedef GVariant * (*get_value_func)      (AST                 *ast,
                                           const GVariantType  *type,
                                           GError             **error);
typedef GVariant * (*get_base_value_func) (AST                 *ast,
                                           const GVariantType  *type,
                                           GError             **error);
typedef void       (*free_func)           (AST                 *ast);

typedef struct
{
  gchar *    (* get_pattern)    (AST                 *ast,
                                 GError             **error);
  GVariant * (* get_value)      (AST                 *ast,
                                 const GVariantType  *type,
                                 GError             **error);
  GVariant * (* get_base_value) (AST                 *ast,
                                 const GVariantType  *type,
                                 GError             **error);
  void       (* free)           (AST                 *ast);
} ASTClass;

struct _AST
{
  const ASTClass *class;
  SourceRef source_ref;
};

static gchar *
ast_get_pattern (AST     *ast,
                 GError **error)
{
  return ast->class->get_pattern (ast, error);
}

static GVariant *
ast_get_value (AST                 *ast,
               const GVariantType  *type,
               GError             **error)
{
  return ast->class->get_value (ast, type, error);
}

static void
ast_free (AST *ast)
{
  ast->class->free (ast);
}

static void
ast_set_error (AST          *ast,
               GError      **error,
               AST          *other_ast,
               const gchar  *format,
               ...)
{
  va_list ap;

  va_start (ap, format);
  parser_set_error_va (error, &ast->source_ref,
                       other_ast ? & other_ast->source_ref : NULL,
                       format, ap);
  va_end (ap);
}

static GVariant *
ast_type_error (AST                 *ast,
                const GVariantType  *type,
                GError             **error)
{
  gchar *typestr;

  typestr = g_variant_type_dup_string (type);
  ast_set_error (ast, error, NULL,
                 "can not parse as value of type `%s'",
                 typestr);
  g_free (typestr);

  return NULL;
}

static GVariant *
ast_resolve (AST     *ast,
             GError **error)
{
  GVariant *value;
  gchar *pattern;
  gint i, j = 0;

  pattern = ast_get_pattern (ast, error);

  if (pattern == NULL)
    return NULL;

  /* choose reasonable defaults
   *
   *   1) favour non-maybe values where possible
   *   2) default type for strings is 's'
   *   3) default type for integers is 'i'
   */
  for (i = 0; pattern[i]; i++)
    switch (pattern[i])
      {
      case '*':
        ast_set_error (ast, error, NULL, "unable to infer type");
        g_free (pattern);
        return NULL;

      case 'M':
        break;

      case 'S':
        pattern[j++] = 's';
        break;

      case 'N':
        pattern[j++] = 'i';
        break;

      default:
        pattern[j++] = pattern[i];
        break;
      }
  pattern[j++] = '\0';

  value = ast_get_value (ast, G_VARIANT_TYPE (pattern), error);
  g_free (pattern);

  return value;
}


static AST *parse (TokenStream  *stream,
                   va_list      *app,
                   GError      **error);

static void
ast_array_append (AST  ***array,
                  gint   *n_items,
                  AST    *ast)
{
  if ((*n_items & (*n_items - 1)) == 0)
    *array = g_renew (AST *, *array, *n_items ? 2 ** n_items : 1);

  (*array)[(*n_items)++] = ast;
}

static void
ast_array_free (AST  **array,
                gint   n_items)
{
  gint i;

  for (i = 0; i < n_items; i++)
    ast_free (array[i]);
  g_free (array);
}

static gchar *
ast_array_get_pattern (AST    **array,
                       gint     n_items,
                       GError **error)
{
  gchar *pattern;
  gint i;

  pattern = ast_get_pattern (array[0], error);

  if (pattern == NULL)
    return NULL;

  for (i = 1; i < n_items; i++)
    {
      gchar *tmp, *merged;

      tmp = ast_get_pattern (array[i], error);

      if (tmp == NULL)
        {
          g_free (pattern);
          return NULL;
        }

      merged = pattern_coalesce (pattern, tmp);
      g_free (pattern);
      pattern = merged;

      if (merged == NULL)
        /* set coalescence implies pairwise coalescence (i think).
         * we should therefore be able to trace the failure to a single
         * pair of values.
         */
        {
          int j = 0;

          while (TRUE)
            {
              gchar *tmp2;
              gchar *m;

              /* if 'j' reaches 'i' then we failed to find the pair */
              g_assert (j < i);

              tmp2 = ast_get_pattern (array[j], NULL);
              g_assert (tmp2 != NULL);

              m = pattern_coalesce (tmp, tmp2);
              g_free (tmp2);
              g_free (m);

              if (m == NULL)
                {
                  /* we found a conflict between 'i' and 'j'.
                   *
                   * report the error.  note: 'j' is first.
                   */
                  ast_set_error (array[j], error, array[i],
                                 "unable to find a common type");
                  g_free (tmp);
                  return NULL;
                }

              j++;
            }
        }
    }

  return pattern;
}

typedef struct
{
  AST ast;

  AST *child;
} Maybe;

static gchar *
maybe_get_pattern (AST     *ast,
                   GError **error)
{
  Maybe *maybe = (Maybe *) ast;

  if (maybe->child != NULL)
    {
      gchar *child_pattern;
      gchar *pattern;

      child_pattern = ast_get_pattern (maybe->child, error);

      if (child_pattern == NULL)
        return NULL;

      pattern = g_strdup_printf ("m%s", child_pattern);
      g_free (child_pattern);

      return pattern;
    }

  return g_strdup ("m*");
}

static GVariant *
maybe_get_value (AST                 *ast,
                 const GVariantType  *type,
                 GError             **error)
{
  Maybe *maybe = (Maybe *) ast;
  GVariant *value;

  if (!g_variant_type_is_maybe (type))
    return ast_type_error (ast, type, error);

  type = g_variant_type_element (type);

  if (maybe->child)
    {
      value = ast_get_value (maybe->child, type, error);

      if (value == NULL)
        return NULL;
    }
  else
    value = NULL;

  return g_variant_new_maybe (type, value);
}

static void
maybe_free (AST *ast)
{
  Maybe *maybe = (Maybe *) ast;

  if (maybe->child != NULL)
    ast_free (maybe->child);

  g_slice_free (Maybe, maybe);
}

static AST *
maybe_parse (TokenStream  *stream,
             va_list      *app,
             GError      **error)
{
  static const ASTClass maybe_class = {
    maybe_get_pattern,
    maybe_get_value, NULL,
    maybe_free
  };
  AST *child = NULL;
  Maybe *maybe;

  if (token_stream_consume (stream, "just"))
    {
      child = parse (stream, app, error);
      if (child == NULL)
        return NULL;
    }

  else if (!token_stream_consume (stream, "nothing"))
    {
      token_stream_set_error (stream, error, TRUE, "unknown keyword");
      return NULL;
    }

  maybe = g_slice_new (Maybe);
  maybe->ast.class = &maybe_class;
  maybe->child = child;

  return (AST *) maybe;
}

static GVariant *
maybe_wrapper (AST                 *ast,
               const GVariantType  *type,
               GError             **error)
{
  const GVariantType *t;
  GVariant *value;
  int depth;

  for (depth = 0, t = type;
       g_variant_type_is_maybe (t);
       depth++, t = g_variant_type_element (t));

  value = ast->class->get_base_value (ast, t, error);

  if (value == NULL)
    return NULL;

  while (depth--)
    value = g_variant_new_maybe (NULL, value);

  return value;
}

typedef struct
{
  AST ast;

  AST **children;
  gint n_children;
} Array;

static gchar *
array_get_pattern (AST     *ast,
                   GError **error)
{
  Array *array = (Array *) ast;
  gchar *pattern;
  gchar *result;

  if (array->n_children == 0)
    return g_strdup ("Ma*");

  pattern = ast_array_get_pattern (array->children, array->n_children, error);

  if (pattern == NULL)
    return NULL;

  result = g_strdup_printf ("Ma%s", pattern);
  g_free (pattern);

  return result;
}

static GVariant *
array_get_value (AST                 *ast,
                 const GVariantType  *type,
                 GError             **error)
{
  Array *array = (Array *) ast;
  const GVariantType *childtype;
  GVariantBuilder builder;
  gint i;

  if (!g_variant_type_is_array (type))
    return ast_type_error (ast, type, error);

  g_variant_builder_init (&builder, type);
  childtype = g_variant_type_element (type);

  for (i = 0; i < array->n_children; i++)
    {
      GVariant *child;

      if (!(child = ast_get_value (array->children[i], childtype, error)))
        {
          g_variant_builder_clear (&builder);
          return NULL;
        }

      g_variant_builder_add_value (&builder, child);
    }

  return g_variant_builder_end (&builder);
}

static void
array_free (AST *ast)
{
  Array *array = (Array *) ast;

  ast_array_free (array->children, array->n_children);
  g_slice_free (Array, array);
}

static AST *
array_parse (TokenStream  *stream,
             va_list      *app,
             GError      **error)
{
  static const ASTClass array_class = {
    array_get_pattern,
    maybe_wrapper, array_get_value,
    array_free
  };
  gboolean need_comma = FALSE;
  Array *array;

  array = g_slice_new (Array);
  array->ast.class = &array_class;
  array->children = NULL;
  array->n_children = 0;

  token_stream_assert (stream, "[");
  while (!token_stream_consume (stream, "]"))
    {
      AST *child;

      if (need_comma &&
          !token_stream_require (stream, ",",
                                 " or `]' to follow array element",
                                 error))
        goto error;

      child = parse (stream, app, error);

      if (!child)
        goto error;

      ast_array_append (&array->children, &array->n_children, child);
      need_comma = TRUE;
    }

  return (AST *) array;

 error:
  ast_array_free (array->children, array->n_children);
  g_slice_free (Array, array);

  return NULL;
}

typedef struct
{
  AST ast;

  AST **children;
  gint n_children;
} Tuple;

static gchar *
tuple_get_pattern (AST     *ast,
                   GError **error)
{
  Tuple *tuple = (Tuple *) ast;
  gchar *result = NULL;
  gchar **parts;
  gint i;

  parts = g_new (gchar *, tuple->n_children + 4);
  parts[tuple->n_children + 1] = (gchar *) ")";
  parts[tuple->n_children + 2] = NULL;
  parts[0] = (gchar *) "M(";

  for (i = 0; i < tuple->n_children; i++)
    if (!(parts[i + 1] = ast_get_pattern (tuple->children[i], error)))
      break;

  if (i == tuple->n_children)
    result = g_strjoinv ("", parts);

  /* parts[0] should not be freed */
  while (i)
    g_free (parts[i--]);
  g_free (parts);

  return result;
}

static GVariant *
tuple_get_value (AST                 *ast,
                 const GVariantType  *type,
                 GError             **error)
{
  Tuple *tuple = (Tuple *) ast;
  const GVariantType *childtype;
  GVariantBuilder builder;
  gint i;

  if (!g_variant_type_is_tuple (type))
    return ast_type_error (ast, type, error);

  g_variant_builder_init (&builder, type);
  childtype = g_variant_type_first (type);

  for (i = 0; i < tuple->n_children; i++)
    {
      GVariant *child;

      if (!(child = ast_get_value (tuple->children[i], childtype, error)))
        {
          g_variant_builder_clear (&builder);
          return FALSE;
        }

      g_variant_builder_add_value (&builder, child);
      childtype = g_variant_type_next (childtype);
    }

  return g_variant_builder_end (&builder);
}

static void
tuple_free (AST *ast)
{
  Tuple *tuple = (Tuple *) ast;

  ast_array_free (tuple->children, tuple->n_children);
  g_slice_free (Tuple, tuple);
}

static AST *
tuple_parse (TokenStream  *stream,
             va_list      *app,
             GError      **error)
{
  static const ASTClass tuple_class = {
    tuple_get_pattern,
    maybe_wrapper, tuple_get_value,
    tuple_free
  };
  gboolean need_comma = FALSE;
  gboolean first = TRUE;
  Tuple *tuple;

  tuple = g_slice_new (Tuple);
  tuple->ast.class = &tuple_class;
  tuple->children = NULL;
  tuple->n_children = 0;

  token_stream_assert (stream, "(");
  while (!token_stream_consume (stream, ")"))
    {
      AST *child;

      if (need_comma &&
          !token_stream_require (stream, ",",
                                 " or `)' to follow tuple element",
                                 error))
        goto error;

      child = parse (stream, app, error);

      if (!child)
        goto error;

      ast_array_append (&tuple->children, &tuple->n_children, child);

      /* the first time, we absolutely require a comma, so grab it here
       * and leave need_comma = FALSE so that the code above doesn't
       * require a second comma.
       *
       * the second and remaining times, we set need_comma = TRUE.
       */
      if (first)
        {
          if (!token_stream_require (stream, ",",
                                     " after first tuple element", error))
            goto error;

          first = FALSE;
        }
      else
        need_comma = TRUE;
    }

  return (AST *) tuple;

 error:
  ast_array_free (tuple->children, tuple->n_children);
  g_slice_free (Tuple, tuple);

  return NULL;
}

typedef struct
{
  AST ast;

  AST *value;
} Variant;

static gchar *
variant_get_pattern (AST     *ast,
                     GError **error)
{
  return g_strdup ("Mv");
}

static GVariant *
variant_get_value (AST                 *ast,
                   const GVariantType  *type,
                   GError             **error)
{
  Variant *variant = (Variant *) ast;
  GVariant *child;

  g_assert (g_variant_type_equal (type, G_VARIANT_TYPE_VARIANT));
  child = ast_resolve (variant->value, error);

  if (child == NULL)
    return NULL;

  return g_variant_new_variant (child);
}

static void
variant_free (AST *ast)
{
  Variant *variant = (Variant *) ast;

  ast_free (variant->value);
  g_slice_free (Variant, variant);
}

static AST *
variant_parse (TokenStream  *stream,
               va_list      *app,
               GError      **error)
{
  static const ASTClass variant_class = {
    variant_get_pattern,
    maybe_wrapper, variant_get_value,
    variant_free
  };
  Variant *variant;
  AST *value;

  token_stream_assert (stream, "<");
  value = parse (stream, app, error);

  if (!value)
    return NULL;

  if (!token_stream_require (stream, ">", " to follow variant value", error))
    {
      ast_free (value);
      return NULL;
    }

  variant = g_slice_new (Variant);
  variant->ast.class = &variant_class;
  variant->value = value;

  return (AST *) variant;
}

typedef struct
{
  AST ast;

  AST **keys;
  AST **values;
  gint n_children;
} Dictionary;

static gchar *
dictionary_get_pattern (AST     *ast,
                        GError **error)
{
  Dictionary *dict = (Dictionary *) ast;
  gchar *value_pattern;
  gchar *key_pattern;
  gchar key_char;
  gchar *result;

  if (dict->n_children == 0)
    return g_strdup ("Ma{**}");

  key_pattern = ast_array_get_pattern (dict->keys,
                                       abs (dict->n_children),
                                       error);

  if (key_pattern == NULL)
    return NULL;

  /* we can not have maybe keys */
  if (key_pattern[0] == 'M')
    key_char = key_pattern[1];
  else
    key_char = key_pattern[0];

  g_free (key_pattern);

  /* the basic types,
   * plus undetermined number type and undetermined string type.
   */
  if (!strchr ("bynqiuxthdsogNS", key_char))
    {
      ast_set_error (ast, error, NULL,
                     "dictionary keys must have basic types");
      return NULL;
    }

  value_pattern = ast_get_pattern (dict->values[0], error);

  if (value_pattern == NULL)
    return NULL;

  result = g_strdup_printf ("M%s{%c%s}",
                            dict->n_children > 0 ? "a" : "",
                            key_char, value_pattern);
  g_free (value_pattern);

  return result;
}

static GVariant *
dictionary_get_value (AST                 *ast,
                      const GVariantType  *type,
                      GError             **error)
{
  Dictionary *dict = (Dictionary *) ast;

  if (dict->n_children == -1)
    {
      const GVariantType *subtype;
      GVariantBuilder builder;
      GVariant *subvalue;

      if (!g_variant_type_is_dict_entry (type))
        return ast_type_error (ast, type, error);

      g_variant_builder_init (&builder, type);

      subtype = g_variant_type_key (type);
      if (!(subvalue = ast_get_value (dict->keys[0], subtype, error)))
        {
          g_variant_builder_clear (&builder);
          return FALSE;
        }
      g_variant_builder_add_value (&builder, subvalue);

      subtype = g_variant_type_value (type);
      if (!(subvalue = ast_get_value (dict->values[0], subtype, error)))
        {
          g_variant_builder_clear (&builder);
          return FALSE;
        }
      g_variant_builder_add_value (&builder, subvalue);

      return g_variant_builder_end (&builder);
    }
  else
    {
      const GVariantType *entry, *key, *val;
      GVariantBuilder builder;
      gint i;

      if (!g_variant_type_is_subtype_of (type, G_VARIANT_TYPE_DICTIONARY))
        return ast_type_error (ast, type, error);

      entry = g_variant_type_element (type);
      key = g_variant_type_key (entry);
      val = g_variant_type_value (entry);

      g_variant_builder_init (&builder, type);

      for (i = 0; i < dict->n_children; i++)
        {
          GVariant *subvalue;

          g_variant_builder_open (&builder, entry);

          if (!(subvalue = ast_get_value (dict->keys[i], key, error)))
            {
              g_variant_builder_clear (&builder);
              return FALSE;
            }
          g_variant_builder_add_value (&builder, subvalue);

          if (!(subvalue = ast_get_value (dict->values[i], val, error)))
            {
              g_variant_builder_clear (&builder);
              return FALSE;
            }
          g_variant_builder_add_value (&builder, subvalue);
          g_variant_builder_close (&builder);
        }

      return g_variant_builder_end (&builder);
    }
}

static void
dictionary_free (AST *ast)
{
  Dictionary *dict = (Dictionary *) ast;
  gint n_children;

  if (dict->n_children > -1)
    n_children = dict->n_children;
  else
    n_children = 1;

  ast_array_free (dict->keys, n_children);
  ast_array_free (dict->values, n_children);
  g_slice_free (Dictionary, dict);
}

static AST *
dictionary_parse (TokenStream  *stream,
                  va_list      *app,
                  GError      **error)
{
  static const ASTClass dictionary_class = {
    dictionary_get_pattern,
    maybe_wrapper, dictionary_get_value,
    dictionary_free
  };
  gint n_keys, n_values;
  gboolean only_one;
  Dictionary *dict;
  AST *first;

  dict = g_slice_new (Dictionary);
  dict->ast.class = &dictionary_class;
  dict->keys = NULL;
  dict->values = NULL;
  n_keys = n_values = 0;

  token_stream_assert (stream, "{");

  if (token_stream_consume (stream, "}"))
    {
      dict->n_children = 0;
      return (AST *) dict;
    }

  if ((first = parse (stream, app, error)) == NULL)
    goto error;

  ast_array_append (&dict->keys, &n_keys, first);

  only_one = token_stream_consume (stream, ",");
  if (!only_one &&
      !token_stream_require (stream, ":",
                             " or `,' to follow dictionary entry key",
                             error))
    goto error;

  if ((first = parse (stream, app, error)) == NULL)
    goto error;

  ast_array_append (&dict->values, &n_values, first);

  if (only_one)
    {
      if (!token_stream_require (stream, "}", " at end of dictionary entry",
                                 error))
        goto error;

      g_assert (n_keys == 1 && n_values == 1);
      dict->n_children = -1;

      return (AST *) dict;
    }

  while (!token_stream_consume (stream, "}"))
    {
      AST *child;

      if (!token_stream_require (stream, ",",
                                 " or `}' to follow dictionary entry", error))
        goto error;

      child = parse (stream, app, error);

      if (!child)
        goto error;

      ast_array_append (&dict->keys, &n_keys, child);

      if (!token_stream_require (stream, ":",
                                 " to follow dictionary entry key", error))
        goto error;

      child = parse (stream, app, error);

      if (!child)
        goto error;

      ast_array_append (&dict->values, &n_values, child);
    }

  g_assert (n_keys == n_values);
  dict->n_children = n_keys;

  return (AST *) dict;

 error:
  ast_array_free (dict->keys, n_keys);
  ast_array_free (dict->values, n_values);
  g_slice_free (Dictionary, dict);

  return NULL;
}

typedef struct
{
  AST ast;
  gchar *string;
} String;

static gchar *
string_get_pattern (AST     *ast,
                    GError **error)
{
  return g_strdup ("MS");
}

static GVariant *
string_get_value (AST                 *ast,
                  const GVariantType  *type,
                  GError             **error)
{
  String *string = (String *) ast;

  if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    return g_variant_new_string (string->string);

  else if (g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH))
    {
      if (!g_variant_is_object_path (string->string))
        {
          ast_set_error (ast, error, NULL, "not a valid object path");
          return NULL;
        }

      return g_variant_new_object_path (string->string);
    }

  else if (g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
    {
      if (!g_variant_is_signature (string->string))
        {
          ast_set_error (ast, error, NULL, "not a valid signature");
          return NULL;
        }

      return g_variant_new_signature (string->string);
    }

  else
    return ast_type_error (ast, type, error);
}

static void
string_free (AST *ast)
{
  String *string = (String *) ast;

  g_free (string->string);
  g_slice_free (String, string);
}

static AST *
string_parse (TokenStream  *stream,
              va_list      *app,
              GError      **error)
{
  static const ASTClass string_class = {
    string_get_pattern,
    maybe_wrapper, string_get_value,
    string_free
  };
  String *string;
  SourceRef ref;
  gchar *token;
  gsize length;
  gchar quote;
  gchar *str;
  gint i, j;

  token_stream_start_ref (stream, &ref);
  token = token_stream_get (stream);
  token_stream_end_ref (stream, &ref);
  length = strlen (token);
  quote = token[0];

  str = g_malloc (length);
  g_assert (quote == '"' || quote == '\'');
  j = 0;
  i = 1;
  while (token[i] != quote)
    switch (token[i])
      {
      case '\0':
        parser_set_error (error, &ref, NULL,
                          "unterminated string constant");
        g_free (token);
        return NULL;

      case '\\':
        switch (token[++i])
          {
          case '\0':
            parser_set_error (error, &ref, NULL,
                              "unterminated string constant");
            g_free (token);
            return NULL;

          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7':
            {
              /* up to 3 characters */
              guchar val = token[i++] - '0';

              if ('0' <= token[i] && token[i] < '8')
                val = (val << 3) | (token[i++] - '0');

              if ('0' <= token[i] && token[i] < '8')
                val = (val << 3) | (token[i++] - '0');

              str[j++] = val;
            }
            continue;

          case 'b': str[j++] = '\b'; i++; continue;
          case 'f': str[j++] = '\f'; i++; continue;
          case 'n': str[j++] = '\n'; i++; continue;
          case 'r': str[j++] = '\r'; i++; continue;
          case 't': str[j++] = '\t'; i++; continue;
          case '\n': i++; continue;
          }

      default:
        str[j++] = token[i++];
      }
  str[j++] = '\0';
  g_free (token);

  string = g_slice_new (String);
  string->ast.class = &string_class;
  string->string = str;

  token_stream_next (stream);

  return (AST *) string;
}

typedef struct
{
  AST ast;

  gchar *token;
} Number;

static gchar *
number_get_pattern (AST     *ast,
                    GError **error)
{
  Number *number = (Number *) ast;

  if (strchr (number->token, '.') ||
      (!g_str_has_prefix (number->token, "0x") &&
       strchr (number->token, 'e')))
    return g_strdup ("Md");

  return g_strdup ("MN");
}

static GVariant *
number_overflow (AST                 *ast,
                 const GVariantType  *type,
                 GError             **error)
{
  ast_set_error (ast, error, NULL, "number out of range for type `%c'",
                 g_variant_type_peek_string (type)[0]);
  return NULL;
}

static GVariant *
number_get_value (AST                 *ast,
                  const GVariantType  *type,
                  GError             **error)
{
  Number *number = (Number *) ast;
  const gchar *token;
  gboolean negative;
  gboolean floating;
  guint64 abs_val;
  gdouble dbl_val;
  gchar *end;

  token = number->token;

  if (g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
    {
      floating = TRUE;

      errno = 0;
      dbl_val = g_ascii_strtod (token, &end);
      if (dbl_val != 0.0 && errno == ERANGE)
        {
          ast_set_error (ast, error, NULL, "number too big for any type");
          return NULL;
        }

      /* silence uninitialised warnings... */
      negative = FALSE;
      abs_val = 0;
    }
  else
    {
      floating = FALSE;
      negative = token[0] == '-';
      if (token[0] == '-')
        token++;

      errno = 0;
      abs_val = g_ascii_strtoull (token, &end, 0);
      if (abs_val == G_MAXUINT64 && errno == ERANGE)
        {
          ast_set_error (ast, error, NULL, "integer too big for any type");
          return NULL;
        }

      if (abs_val == 0)
        negative = FALSE;

      /* silence uninitialised warning... */
      dbl_val = 0.0;
    }

  if (*end != '\0')
    {
      SourceRef ref;

      ref = ast->source_ref;
      ref.start += end - number->token;
      ref.end = ref.start + 1;

      parser_set_error (error, &ref, NULL,
                        "invalid character in number");
      return NULL;
     }

  if (floating)
    return g_variant_new_double (dbl_val);

  switch (*g_variant_type_peek_string (type))
    {
    case 'y':
      if (negative || abs_val > G_MAXUINT8)
        return number_overflow (ast, type, error);
      return g_variant_new_byte (abs_val);

    case 'n':
      if (abs_val - negative > G_MAXINT16)
        return number_overflow (ast, type, error);
      return g_variant_new_int16 (negative ? -abs_val : abs_val);

    case 'q':
      if (negative || abs_val > G_MAXUINT16)
        return number_overflow (ast, type, error);
      return g_variant_new_uint16 (negative ? -abs_val : abs_val);

    case 'i':
      if (abs_val - negative > G_MAXINT32)
        return number_overflow (ast, type, error);
      return g_variant_new_int32 (negative ? -abs_val : abs_val);

    case 'u':
      if (negative || abs_val > G_MAXUINT32)
        return number_overflow (ast, type, error);
      return g_variant_new_uint32 (negative ? -abs_val : abs_val);

    case 'x':
      if (abs_val - negative > G_MAXINT64)
        return number_overflow (ast, type, error);
      return g_variant_new_int64 (negative ? -abs_val : abs_val);

    case 't':
      if (negative)
        return number_overflow (ast, type, error);
      return g_variant_new_uint64 (negative ? -abs_val : abs_val);

    case 'h':
      if (abs_val - negative > G_MAXINT32)
        return number_overflow (ast, type, error);
      return g_variant_new_handle (negative ? -abs_val : abs_val);

    default:
      return ast_type_error (ast, type, error);
    }
}

static void
number_free (AST *ast)
{
  Number *number = (Number *) ast;

  g_free (number->token);
  g_slice_free (Number, number);
}

static AST *
number_parse (TokenStream  *stream,
              va_list      *app,
              GError      **error)
{
  static const ASTClass number_class = {
    number_get_pattern,
    maybe_wrapper, number_get_value,
    number_free
  };
  Number *number;

  number = g_slice_new (Number);
  number->ast.class = &number_class;
  number->token = token_stream_get (stream);
  token_stream_next (stream);

  return (AST *) number;
}

typedef struct
{
  AST ast;
  gboolean value;
} Boolean;

static gchar *
boolean_get_pattern (AST     *ast,
                     GError **error)
{
  return g_strdup ("Mb");
}

static GVariant *
boolean_get_value (AST                 *ast,
                   const GVariantType  *type,
                   GError             **error)
{
  Boolean *boolean = (Boolean *) ast;

  if (!g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    return ast_type_error (ast, type, error);

  return g_variant_new_boolean (boolean->value);
}

static void
boolean_free (AST *ast)
{
  Boolean *boolean = (Boolean *) ast;

  g_slice_free (Boolean, boolean);
}

static AST *
boolean_new (gboolean value)
{
  static const ASTClass boolean_class = {
    boolean_get_pattern,
    maybe_wrapper, boolean_get_value,
    boolean_free
  };
  Boolean *boolean;

  boolean = g_slice_new (Boolean);
  boolean->ast.class = &boolean_class;
  boolean->value = value;

  return (AST *) boolean;
}

typedef struct
{
  AST ast;

  GVariant *value;
} Positional;

static gchar *
positional_get_pattern (AST     *ast,
                        GError **error)
{
  Positional *positional = (Positional *) ast;

  return g_strdup (g_variant_get_type_string (positional->value));
}

static GVariant *
positional_get_value (AST                 *ast,
                      const GVariantType  *type,
                      GError             **error)
{
  Positional *positional = (Positional *) ast;
  GVariant *value;

  g_assert (positional->value != NULL);

  if G_UNLIKELY (!g_variant_is_of_type (positional->value, type))
    return ast_type_error (ast, type, error);

  /* NOTE: if _get is called more than once then
   * things get messed up with respect to floating refs.
   *
   * fortunately, this function should only ever get called once.
   */
  g_assert (positional->value != NULL);
  value = positional->value;
  positional->value = NULL;

  return value;
}

static void
positional_free (AST *ast)
{
  Positional *positional = (Positional *) ast;

  /* if positional->value is set, just leave it.
   * memory management doesn't matter in case of programmer error.
   */
  g_slice_free (Positional, positional);
}

static AST *
positional_parse (TokenStream  *stream,
                  va_list      *app,
                  GError      **error)
{
  static const ASTClass positional_class = {
    positional_get_pattern,
    positional_get_value, NULL,
    positional_free
  };
  Positional *positional;
  const gchar *endptr;
  gchar *token;

  token = token_stream_get (stream);
  g_assert (token[0] == '%');

  positional = g_slice_new (Positional);
  positional->ast.class = &positional_class;
  positional->value = g_variant_new_va (token + 1, &endptr, app);

  if (*endptr || positional->value == NULL)
    {
      token_stream_set_error (stream, error, TRUE,
                              "invalid GVariant format string");
      /* memory management doesn't matter in case of programmer error. */
      return NULL;
    }

  token_stream_next (stream);
  g_free (token);

  return (AST *) positional;
}

typedef struct
{
  AST ast;

  GVariantType *type;
  AST *child;
} TypeDecl;

static gchar *
typedecl_get_pattern (AST     *ast,
                      GError **error)
{
  TypeDecl *decl = (TypeDecl *) ast;

  return g_variant_type_dup_string (decl->type);
}

static GVariant *
typedecl_get_value (AST                 *ast,
                    const GVariantType  *type,
                    GError             **error)
{
  TypeDecl *decl = (TypeDecl *) ast;

  return ast_get_value (decl->child, type, error);
}

static void
typedecl_free (AST *ast)
{
  TypeDecl *decl = (TypeDecl *) ast;

  ast_free (decl->child);
  g_variant_type_free (decl->type);
  g_slice_free (TypeDecl, decl);
}

static AST *
typedecl_parse (TokenStream  *stream,
                va_list      *app,
                GError      **error)
{
  static const ASTClass typedecl_class = {
    typedecl_get_pattern,
    typedecl_get_value, NULL,
    typedecl_free
  };
  GVariantType *type;
  TypeDecl *decl;
  AST *child;

  if (token_stream_peek (stream, '@'))
    {
      gchar *token;

      token = token_stream_get (stream);

      if (!g_variant_type_string_is_valid (token + 1))
        {
          token_stream_set_error (stream, error, TRUE,
                                  "invalid type declaration");
          g_free (token);

          return NULL;
        }

      type = g_variant_type_new (token + 1);

      if (!g_variant_type_is_definite (type))
        {
          token_stream_set_error (stream, error, TRUE,
                                  "type declarations must be definite");
          g_variant_type_free (type);
          g_free (token);

          return NULL;
        }

      token_stream_next (stream);
      g_free (token);
    }
  else
    {
      if (token_stream_consume (stream, "boolean"))
        type = g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);

      else if (token_stream_consume (stream, "byte"))
        type = g_variant_type_copy (G_VARIANT_TYPE_BYTE);

      else if (token_stream_consume (stream, "int16"))
        type = g_variant_type_copy (G_VARIANT_TYPE_INT16);

      else if (token_stream_consume (stream, "uint16"))
        type = g_variant_type_copy (G_VARIANT_TYPE_UINT16);

      else if (token_stream_consume (stream, "int32"))
        type = g_variant_type_copy (G_VARIANT_TYPE_INT32);

      else if (token_stream_consume (stream, "handle"))
        type = g_variant_type_copy (G_VARIANT_TYPE_HANDLE);

      else if (token_stream_consume (stream, "uint32"))
        type = g_variant_type_copy (G_VARIANT_TYPE_UINT32);

      else if (token_stream_consume (stream, "int64"))
        type = g_variant_type_copy (G_VARIANT_TYPE_INT64);

      else if (token_stream_consume (stream, "uint64"))
        type = g_variant_type_copy (G_VARIANT_TYPE_UINT64);

      else if (token_stream_consume (stream, "double"))
        type = g_variant_type_copy (G_VARIANT_TYPE_DOUBLE);

      else if (token_stream_consume (stream, "string"))
        type = g_variant_type_copy (G_VARIANT_TYPE_STRING);

      else if (token_stream_consume (stream, "objectpath"))
        type = g_variant_type_copy (G_VARIANT_TYPE_OBJECT_PATH);

      else if (token_stream_consume (stream, "signature"))
        type = g_variant_type_copy (G_VARIANT_TYPE_SIGNATURE);

      else
        {
          token_stream_set_error (stream, error, TRUE, "unknown keyword");
          return NULL;
        }
    }

  if ((child = parse (stream, app, error)) == NULL)
    {
      g_variant_type_free (type);
      return NULL;
    }

  decl = g_slice_new (TypeDecl);
  decl->ast.class = &typedecl_class;
  decl->type = type;
  decl->child = child;

  return (AST *) decl;
}

static AST *
parse (TokenStream  *stream,
       va_list      *app,
       GError      **error)
{
  SourceRef source_ref;
  AST *result;

  token_stream_prepare (stream);
  token_stream_start_ref (stream, &source_ref);

  if (token_stream_peek (stream, '['))
    result = array_parse (stream, app, error);

  else if (token_stream_peek (stream, '('))
    result = tuple_parse (stream, app, error);

  else if (token_stream_peek (stream, '<'))
    result = variant_parse (stream, app, error);

  else if (token_stream_peek (stream, '{'))
    result = dictionary_parse (stream, app, error);

  else if (app && token_stream_peek (stream, '%'))
    result = positional_parse (stream, app, error);

  else if (token_stream_consume (stream, "true"))
    result = boolean_new (TRUE);

  else if (token_stream_consume (stream, "false"))
    result = boolean_new (FALSE);

  else if (token_stream_peek (stream, 'n') ||
           token_stream_peek (stream, 'j'))
    result = maybe_parse (stream, app, error);

  else if (token_stream_peek (stream, '@') ||
           token_stream_is_keyword (stream))
    result = typedecl_parse (stream, app, error);

  else if (token_stream_is_numeric (stream))
    result = number_parse (stream, app, error);

  else if (token_stream_peek (stream, '\'') ||
           token_stream_peek (stream, '"'))
    result = string_parse (stream, app, error);

  else
    {
      token_stream_set_error (stream, error, FALSE, "expected value");
      return NULL;
    }

  if (result != NULL)
    {
      token_stream_end_ref (stream, &source_ref);
      result->source_ref = source_ref;
    }

  return result;
}

/**
 * g_variant_parse:
 * @type: a #GVariantType, or %NULL
 * @text: a string containing a GVariant in text form
 * @limit: a pointer to the end of @text, or %NULL
 * @endptr: a location to store the end pointer, or %NULL
 * @error: a pointer to a %NULL #GError pointer, or %NULL
 * @Returns: a reference to a #GVariant, or %NULL
 *
 * Parses a #GVariant from a text representation.
 *
 * A single #GVariant is parsed from the content of @text.
 *
 * The memory at @limit will never be accessed and the parser behaves as
 * if the character at @limit is the nul terminator.  This has the
 * effect of bounding @text.
 *
 * If @endptr is non-%NULL then @text is permitted to contain data
 * following the value that this function parses and @endptr will be
 * updated to point to the first character past the end of the text
 * parsed by this function.  If @endptr is %NULL and there is extra data
 * then an error is returned.
 *
 * If @type is non-%NULL then the value will be parsed to have that
 * type.  This may result in additional parse errors (in the case that
 * the parsed value doesn't fit the type) but may also result in fewer
 * errors (in the case that the type would have been ambiguous, such as
 * with empty arrays).
 *
 * In the event that the parsing is successful, the resulting #GVariant
 * is returned.
 *
 * In case of any error, %NULL will be returned.  If @error is non-%NULL
 * then it will be set to reflect the error that occured.
 *
 * Officially, the language understood by the parser is "any string
 * produced by g_variant_print()".
 **/
GVariant *
g_variant_parse (const GVariantType  *type,
                 const gchar         *text,
                 const gchar         *limit,
                 const gchar        **endptr,
                 GError             **error)
{
  TokenStream stream = { 0, };
  GVariant *result = NULL;
  AST *ast;

  g_return_val_if_fail (text != NULL, NULL);
  g_return_val_if_fail (text == limit || text != NULL, NULL);

  stream.start = text;
  stream.stream = text;
  stream.end = limit;

  if ((ast = parse (&stream, NULL, error)))
    {
      if (type == NULL)
        result = ast_resolve (ast, error);
      else
        result = ast_get_value (ast, type, error);

      if (result != NULL)
        {
          g_variant_ref_sink (result);

          if (endptr == NULL)
            {
              while (stream.stream != limit &&
                     g_ascii_isspace (*stream.stream))
                stream.stream++;

              if (stream.stream != limit && *stream.stream != '\0')
                {
                  SourceRef ref = { stream.stream - text,
                                    stream.stream - text };

                  parser_set_error (error, &ref, NULL,
                                    "expected end of input");
                  g_variant_unref (result);

                  result = NULL;
                }
            }
          else
            *endptr = stream.stream;
        }

      ast_free (ast);
    }

  return result;
}

/**
 * g_variant_new_parsed_va:
 * @format: a text format #GVariant
 * @app: a pointer to a #va_list
 * @returns: a new, usually floating, #GVariant
 *
 * Parses @format and returns the result.
 *
 * This is the version of g_variant_new_parsed() intended to be used
 * from libraries.
 *
 * The return value will be floating if it was a newly created GVariant
 * instance.  In the case that @format simply specified the collection
 * of a #GVariant pointer (eg: @format was "%*") then the collected
 * #GVariant pointer will be returned unmodified, without adding any
 * additional references.
 *
 * In order to behave correctly in all cases it is necessary for the
 * calling function to g_variant_ref_sink() the return result before
 * returning control to the user that originally provided the pointer.
 * At this point, the caller will have their own full reference to the
 * result.  This can also be done by adding the result to a container,
 * or by passing it to another g_variant_new() call.
 **/
GVariant *
g_variant_new_parsed_va (const gchar *format,
                         va_list     *app)
{
  TokenStream stream = { 0, };
  GVariant *result = NULL;
  GError *error = NULL;
  AST *ast;

  g_return_val_if_fail (format != NULL, NULL);
  g_return_val_if_fail (app != NULL, NULL);

  stream.start = format;
  stream.stream = format;
  stream.end = NULL;

  if ((ast = parse (&stream, app, &error)))
    {
      result = ast_resolve (ast, &error);
      ast_free (ast);
    }

  if (result == NULL)
    g_error ("g_variant_new_parsed: %s", error->message);

  if (*stream.stream)
    g_error ("g_variant_new_parsed: trailing text after value");

  return result;
}

/**
 * g_variant_new_parsed:
 * @format: a text format #GVariant
 * @...: arguments as per @format
 * @returns: a new floating #GVariant instance
 *
 * Parses @format and returns the result.
 *
 * @format must be a text format #GVariant with one extention: at any
 * point that a value may appear in the text, a '%' character followed
 * by a GVariant format string (as per g_variant_new()) may appear.  In
 * that case, the same arguments are collected from the argument list as
 * g_variant_new() would have collected.
 *
 * Consider this simple example:
 *
 * <informalexample><programlisting>
 *  g_variant_new_parsed ("[('one', 1), ('two', %i), (%s, 3)]", 2, "three");
 * </programlisting></informalexample>
 *
 * In the example, the variable argument parameters are collected and
 * filled in as if they were part of the original string to produce the
 * result of <code>[('one', 1), ('two', 2), ('three', 3)]</code>.
 *
 * This function is intended only to be used with @format as a string
 * literal.  Any parse error is fatal to the calling process.  If you
 * want to parse data from untrusted sources, use g_variant_parse().
 *
 * You may not use this function to return, unmodified, a single
 * #GVariant pointer from the argument list.  ie: @format may not solely
 * be anything along the lines of "%*", "%?", "%r", or anything starting
 * with "%@".
 **/
GVariant *
g_variant_new_parsed (const gchar *format,
                      ...)
{
  GVariant *result;
  va_list ap;

  va_start (ap, format);
  result = g_variant_new_parsed_va (format, &ap);
  va_end (ap);

  return result;
}

#define __G_VARIANT_PARSER_C__
#include "galiasdef.c"
