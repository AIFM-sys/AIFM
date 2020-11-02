/*-
 * Copyright (c) 2000-2008 Poul-Henning Kamp
 * Copyright (c) 2000-2008 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/bsd_cdefs.h>
//__FBSDID("$FreeBSD$");

#include <sys/bsd_param.h>

#ifdef _FREEBSD_KERNEL
#include <sys/bsd_ctype.h>
#include <sys/bsd_kernel.h>
#include <sys/bsd_malloc.h>
#include <sys/bsd_systm.h>
#include <sys/bsd_uio.h>
#include <stdio.h>
#else /* _FREEBSD_KERNEL */
#include <bsd_ctype.h>
#include <bsd_stdarg.h>
#include <bsd_stdio.h>
#include <bsd_stdlib.h>
#include <bsd_string.h>
#endif /* _FREEBSD_KERNEL */

#include <sys/bsd_sbuf.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef _FREEBSD_KERNEL
static MALLOC_DEFINE(M_SBUF, "sbuf", "string buffers");
#define	SBMALLOC(size)		bsd_malloc(size, M_SBUF, M_WAITOK)
#define	SBFREE(buf)		bsd_free(buf, M_SBUF)
#else /* _FREEBSD_KERNEL */
#define	KASSERT(e, m)
#define	SBMALLOC(size)		bsd_malloc(size)
#define	SBFREE(buf)		bsd_free(buf)
#define	min(x,y)		MIN(x,y)
#endif /* _FREEBSD_KERNEL */

/*
 * Predicates
 */
#define	SBUF_ISDYNAMIC(s)	((s)->s_flags & SBUF_DYNAMIC)
#define	SBUF_ISDYNSTRUCT(s)	((s)->s_flags & SBUF_DYNSTRUCT)
#define	SBUF_ISFINISHED(s)	((s)->s_flags & SBUF_FINISHED)
#define	SBUF_HASOVERFLOWED(s)	((s)->s_flags & SBUF_OVERFLOWED)
#define	SBUF_HASROOM(s)		((s)->s_len < (s)->s_size - 1)
#define	SBUF_FREESPACE(s)	((s)->s_size - (s)->s_len - 1)
#define	SBUF_CANEXTEND(s)	((s)->s_flags & SBUF_AUTOEXTEND)

/*
 * Set / clear flags
 */
#define	SBUF_SETFLAG(s, f)	do { (s)->s_flags |= (f); } while (0)
#define	SBUF_CLEARFLAG(s, f)	do { (s)->s_flags &= ~(f); } while (0)

#define	SBUF_MINEXTENDSIZE	16		/* Should be power of 2. */
#define	SBUF_MAXEXTENDSIZE	PAGE_SIZE
#define	SBUF_MAXEXTENDINCR	PAGE_SIZE

/*
 * Debugging support
 */
#if defined(_FREEBSD_KERNEL) && defined(INVARIANTS)

static void
_assert_sbuf_integrity(const char *fun, struct sbuf *s)
{

	KASSERT(s != NULL,
	    ("%s called with a NULL sbuf pointer", fun));
	KASSERT(s->s_buf != NULL,
	    ("%s called with uninitialized or corrupt sbuf", fun));
	KASSERT(s->s_len < s->s_size,
	    ("wrote past end of sbuf (%d >= %d)", s->s_len, s->s_size));
}

static void
_assert_sbuf_state(const char *fun, struct sbuf *s, int state)
{

	KASSERT((s->s_flags & SBUF_FINISHED) == state,
	    ("%s called with %sfinished or corrupt sbuf", fun,
	    (state ? "un" : "")));
}

#define	assert_sbuf_integrity(s) _assert_sbuf_integrity(__func__, (s))
#define	assert_sbuf_state(s, i)	 _assert_sbuf_state(__func__, (s), (i))

#else /* _FREEBSD_KERNEL && INVARIANTS */

#define	assert_sbuf_integrity(s) do { } while (0)
#define	assert_sbuf_state(s, i)	 do { } while (0)

#endif /* _FREEBSD_KERNEL && INVARIANTS */

static int
sbuf_extendsize(int size)
{
	int newsize;

	newsize = SBUF_MINEXTENDSIZE;
	while (newsize < size) {
		if (newsize < (int)SBUF_MAXEXTENDSIZE)
			newsize *= 2;
		else
			newsize += SBUF_MAXEXTENDINCR;
	}
	return (newsize);
}


/*
 * Extend an sbuf.
 */
static int
sbuf_extend(struct sbuf *s, int addlen)
{
	char *newbuf;
	int newsize;

	if (!SBUF_CANEXTEND(s))
		return (-1);
	newsize = sbuf_extendsize(s->s_size + addlen);
	newbuf = SBMALLOC(newsize);
	if (newbuf == NULL)
		return (-1);
	bcopy(s->s_buf, newbuf, s->s_size);
	if (SBUF_ISDYNAMIC(s))
		SBFREE(s->s_buf);
	else
		SBUF_SETFLAG(s, SBUF_DYNAMIC);
	s->s_buf = newbuf;
	s->s_size = newsize;
	return (0);
}

/*
 * Initialize an sbuf.
 * If buf is non-NULL, it points to a static or already-allocated string
 * big enough to hold at least length characters.
 */
struct sbuf *
sbuf_new(struct sbuf *s, char *buf, int length, int flags)
{

	KASSERT(length >= 0,
	    ("attempt to create an sbuf of negative length (%d)", length));
	KASSERT((flags & ~SBUF_USRFLAGMSK) == 0,
	    ("%s called with invalid flags", __func__));

	flags &= SBUF_USRFLAGMSK;
	if (s == NULL) {
		s = SBMALLOC(sizeof(*s));
		if (s == NULL)
			return (NULL);
		bzero(s, sizeof(*s));
		s->s_flags = flags;
		SBUF_SETFLAG(s, SBUF_DYNSTRUCT);
	} else {
		bzero(s, sizeof(*s));
		s->s_flags = flags;
	}
	s->s_size = length;
	if (buf) {
		s->s_buf = buf;
		return (s);
	}
	if (flags & SBUF_AUTOEXTEND)
		s->s_size = sbuf_extendsize(s->s_size);
	s->s_buf = SBMALLOC(s->s_size);
	if (s->s_buf == NULL) {
		if (SBUF_ISDYNSTRUCT(s))
			SBFREE(s);
		return (NULL);
	}
	SBUF_SETFLAG(s, SBUF_DYNAMIC);
	return (s);
}

#ifdef _FREEBSD_KERNEL
/*
 * Create an sbuf with uio data
 */
struct sbuf *
sbuf_uionew(struct sbuf *s, struct uio *uio, int *error)
{

	KASSERT(uio != NULL,
	    ("%s called with NULL uio pointer", __func__));
	KASSERT(error != NULL,
	    ("%s called with NULL error pointer", __func__));

	s = sbuf_new(s, NULL, uio->uio_resid + 1, 0);
	if (s == NULL) {
		*error = ENOMEM;
		return (NULL);
	}
	*error = uiomove(s->s_buf, uio->uio_resid, uio);
	if (*error != 0) {
		sbuf_delete(s);
		return (NULL);
	}
	s->s_len = s->s_size - 1;
	*error = 0;
	return (s);
}
#endif

/*
 * Clear an sbuf and reset its position.
 */
void
sbuf_clear(struct sbuf *s)
{

	assert_sbuf_integrity(s);
	/* don't care if it's finished or not */

	SBUF_CLEARFLAG(s, SBUF_FINISHED);
	SBUF_CLEARFLAG(s, SBUF_OVERFLOWED);
	s->s_len = 0;
}

/*
 * Set the sbuf's end position to an arbitrary value.
 * Effectively truncates the sbuf at the new position.
 */
int
sbuf_setpos(struct sbuf *s, int pos)
{

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	KASSERT(pos >= 0,
	    ("attempt to seek to a negative position (%d)", pos));
	KASSERT(pos < s->s_size,
	    ("attempt to seek past end of sbuf (%d >= %d)", pos, s->s_size));

	if (pos < 0 || pos > s->s_len)
		return (-1);
	s->s_len = pos;
	return (0);
}

/*
 * Append a byte string to an sbuf.
 */
int
sbuf_bcat(struct sbuf *s, const void *buf, size_t len)
{
	const char *str = buf;

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	if (SBUF_HASOVERFLOWED(s))
		return (-1);
	for (; len; len--) {
		if (!SBUF_HASROOM(s) && sbuf_extend(s, len) < 0)
			break;
		s->s_buf[s->s_len++] = *str++;
	}
	if (len) {
		SBUF_SETFLAG(s, SBUF_OVERFLOWED);
		return (-1);
	}
	return (0);
}

#ifdef _FREEBSD_KERNEL
/*
 * Copy a byte string from userland into an sbuf.
 */
int
sbuf_bcopyin(struct sbuf *s, const void *uaddr, size_t len)
{

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	if (SBUF_HASOVERFLOWED(s))
		return (-1);
	if (len == 0)
		return (0);
	if (len > SBUF_FREESPACE(s)) {
		sbuf_extend(s, len - SBUF_FREESPACE(s));
		len = min(len, SBUF_FREESPACE(s));
	}
	if (copyin(uaddr, s->s_buf + s->s_len, len) != 0)
		return (-1);
	s->s_len += len;

	return (0);
}
#endif

/*
 * Copy a byte string into an sbuf.
 */
int
sbuf_bcpy(struct sbuf *s, const void *buf, size_t len)
{

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	sbuf_clear(s);
	return (sbuf_bcat(s, buf, len));
}

/*
 * Append a string to an sbuf.
 */
int
sbuf_cat(struct sbuf *s, const char *str)
{

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	if (SBUF_HASOVERFLOWED(s))
		return (-1);

	while (*str) {
		if (!SBUF_HASROOM(s) && sbuf_extend(s, strlen(str)) < 0)
			break;
		s->s_buf[s->s_len++] = *str++;
	}
	if (*str) {
		SBUF_SETFLAG(s, SBUF_OVERFLOWED);
		return (-1);
	}
	return (0);
}

#ifdef _FREEBSD_KERNEL
/*
 * Append a string from userland to an sbuf.
 */
int
sbuf_copyin(struct sbuf *s, const void *uaddr, size_t len)
{
	size_t done;

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	if (SBUF_HASOVERFLOWED(s))
		return (-1);

	if (len == 0)
		len = SBUF_FREESPACE(s);	/* XXX return 0? */
	if (len > SBUF_FREESPACE(s)) {
		sbuf_extend(s, len);
		len = min(len, SBUF_FREESPACE(s));
	}
	switch (copyinstr(uaddr, s->s_buf + s->s_len, len + 1, &done)) {
	case ENAMETOOLONG:
		SBUF_SETFLAG(s, SBUF_OVERFLOWED);
		/* fall through */
	case 0:
		s->s_len += done - 1;
		break;
	default:
		return (-1);	/* XXX */
	}

	return (done);
}
#endif

/*
 * Copy a string into an sbuf.
 */
int
sbuf_cpy(struct sbuf *s, const char *str)
{

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	sbuf_clear(s);
	return (sbuf_cat(s, str));
}

/*
 * Format the given argument list and append the resulting string to an sbuf.
 */
int
sbuf_vprintf(struct sbuf *s, const char *fmt, va_list ap)
{
	va_list ap_copy;
	int len;

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	KASSERT(fmt != NULL,
	    ("%s called with a NULL format string", __func__));

	if (SBUF_HASOVERFLOWED(s))
		return (-1);

	do {
		va_copy(ap_copy, ap);
		len = vsnprintf(&s->s_buf[s->s_len], SBUF_FREESPACE(s) + 1,
		    fmt, ap_copy);
		va_end(ap_copy);
	} while (len > SBUF_FREESPACE(s) &&
	    sbuf_extend(s, len - SBUF_FREESPACE(s)) == 0);

	/*
	 * s->s_len is the length of the string, without the terminating nul.
	 * When updating s->s_len, we must subtract 1 from the length that
	 * we passed into vsnprintf() because that length includes the
	 * terminating nul.
	 *
	 * vsnprintf() returns the amount that would have been copied,
	 * given sufficient space, hence the min() calculation below.
	 */
	s->s_len += min(len, SBUF_FREESPACE(s));
	if (!SBUF_HASROOM(s) && !SBUF_CANEXTEND(s))
		SBUF_SETFLAG(s, SBUF_OVERFLOWED);

	KASSERT(s->s_len < s->s_size,
	    ("wrote past end of sbuf (%d >= %d)", s->s_len, s->s_size));

	if (SBUF_HASOVERFLOWED(s))
		return (-1);
	return (0);
}

/*
 * Format the given arguments and append the resulting string to an sbuf.
 */
int
sbuf_printf(struct sbuf *s, const char *fmt, ...)
{
	va_list ap;
	int result;

	va_start(ap, fmt);
	result = sbuf_vprintf(s, fmt, ap);
	va_end(ap);
	return (result);
}

/*
 * Append a character to an sbuf.
 */
int
sbuf_putc(struct sbuf *s, int c)
{

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	if (SBUF_HASOVERFLOWED(s))
		return (-1);
	if (!SBUF_HASROOM(s) && sbuf_extend(s, 1) < 0) {
		SBUF_SETFLAG(s, SBUF_OVERFLOWED);
		return (-1);
	}
	if (c != '\0')
	    s->s_buf[s->s_len++] = c;
	return (0);
}

/*
 * Trim whitespace characters from end of an sbuf.
 */
int
sbuf_trim(struct sbuf *s)
{

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	if (SBUF_HASOVERFLOWED(s))
		return (-1);

	while (s->s_len && isspace(s->s_buf[s->s_len-1]))
		--s->s_len;

	return (0);
}

/*
 * Check if an sbuf overflowed
 */
int
sbuf_overflowed(struct sbuf *s)
{

	return (SBUF_HASOVERFLOWED(s));
}

/*
 * Finish off an sbuf.
 */
void
sbuf_finish(struct sbuf *s)
{

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, 0);

	s->s_buf[s->s_len] = '\0';
	SBUF_CLEARFLAG(s, SBUF_OVERFLOWED);
	SBUF_SETFLAG(s, SBUF_FINISHED);
}

/*
 * Return a pointer to the sbuf data.
 */
char *
sbuf_data(struct sbuf *s)
{

	assert_sbuf_integrity(s);
	assert_sbuf_state(s, SBUF_FINISHED);

	return (s->s_buf);
}

/*
 * Return the length of the sbuf data.
 */
int
sbuf_len(struct sbuf *s)
{

	assert_sbuf_integrity(s);
	/* don't care if it's finished or not */

	if (SBUF_HASOVERFLOWED(s))
		return (-1);
	return (s->s_len);
}

/*
 * Clear an sbuf, free its buffer if necessary.
 */
void
sbuf_delete(struct sbuf *s)
{
	int isdyn;

	assert_sbuf_integrity(s);
	/* don't care if it's finished or not */

	if (SBUF_ISDYNAMIC(s))
		SBFREE(s->s_buf);
	isdyn = SBUF_ISDYNSTRUCT(s);
	bzero(s, sizeof(*s));
	if (isdyn)
		SBFREE(s);
}

/*
 * Check if an sbuf has been finished.
 */
int
sbuf_done(struct sbuf *s)
{

	return (SBUF_ISFINISHED(s));
}
