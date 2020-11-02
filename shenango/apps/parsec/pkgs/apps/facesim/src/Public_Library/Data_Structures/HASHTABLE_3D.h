//#####################################################################
// Copyright 2002-2004, Robert Bridson, Ronald Fedkiw, Geoffrey Irving, Neil Molino.
// This file is part of PhysBAM whose distribution is governed by the license contained in the accompanying file PHYSBAM_COPYRIGHT.txt.
//#####################################################################
// Class HASHTABLE_3D
//#####################################################################
//
// The data stored in the table should be lightweight, e.g. integers or pointers, since it could be copied a fair bit.
//
//#####################################################################
#ifndef __HASHTABLE_3D__
#define __HASHTABLE_3D__

#include "../Math_Tools/hash_function.h"
#include "../Matrices_And_Vectors/VECTOR_3D.h"
#include "../Arrays/ARRAY.h"
#include "../Arrays/ARRAYS.h"
namespace PhysBAM
{

template<class T>
class HASHTABLE_3D
{
public:
	ARRAY<int>* table_of_lists; // points to the first element in each list
	ARRAY<int>* links; // links together each element in a list
	ARRAYS<int>* indices; // (i,j,k)
	ARRAY<T>* data;
	int free_list; // points to the first allocated free space available
	int number_of_entries;

	HASHTABLE_3D (const int estimated_max_number_of_entries = 1000)
	{
		Initialize_New_Table (estimated_max_number_of_entries);
	}

	~HASHTABLE_3D()
	{
		delete table_of_lists;
		delete indices;
		delete links;
		delete data;
	}

	void Initialize_New_Table (const int estimated_max_number_of_entries_input)
	{
		int estimated_max_number_of_entries = max (5, estimated_max_number_of_entries_input);
		int number_of_lists = 1;

		while (number_of_lists < max (32, estimated_max_number_of_entries / 5)) number_of_lists *= 2;

		table_of_lists = new ARRAY<int> (number_of_lists);
		indices = new ARRAYS<int> (3, estimated_max_number_of_entries);
		links = new ARRAY<int> (estimated_max_number_of_entries);

		for (int i = 1; i <= links->m - 1; i++) (*links) (i) = i + 1; // originally the free space is linked together and (*links)(links->m)=0

		data = new ARRAY<T> (estimated_max_number_of_entries);
		free_list = 1;
		number_of_entries = 0;
	}

	void Resize_Table (const int estimated_max_number_of_entries_input = 0)
	{
		int estimated_max_number_of_entries = estimated_max_number_of_entries_input;

		if (!estimated_max_number_of_entries) estimated_max_number_of_entries = 3 * number_of_entries / 2;

		ARRAY<int> *old_table_of_lists = table_of_lists, *old_links = links;
		ARRAYS<int> *old_indices = indices;
		ARRAY<T> *old_data = data;
		Initialize_New_Table (estimated_max_number_of_entries);

		for (int h = 1; h <= old_table_of_lists->m; h++) for (int p = (*old_table_of_lists) (h); p != 0; p = (*old_links) (p)) Insert_Entry ( (*old_indices) (1, p), (*old_indices) (2, p), (*old_indices) (3, p), (*old_data) (p));

		delete old_table_of_lists;
		delete old_indices;
		delete old_links;
		delete old_data;
	}

	int Hash (const int i, const int j, const int k) const
	{
		int h = triple_int_hash (i, j, k) % table_of_lists->m;

		if (h <= 0) h += table_of_lists->m;

		return h;
	}

	void Insert_Entry (const int i, const int j, const int k, const T& value) // may insert a duplicate entry for i,j,k
	{
		if (free_list == 0) Resize_Table(); // if no free spots, have to grow

		int new_entry = free_list;
		free_list = (*links) (free_list); // put data into the first free spot
		number_of_entries++;
		(*indices) (1, new_entry) = i;
		(*indices) (2, new_entry) = j;
		(*indices) (3, new_entry) = k;
		(*data) (new_entry) = value;
		int h = Hash (i, j, k);
		(*links) (new_entry) = (*table_of_lists) (h);
		(*table_of_lists) (h) = new_entry;
	} // insert new entry at the beginning of the list

	void Insert_Entry (const VECTOR_3D<int>& v, const T& value)
	{
		Insert_Entry (v.x, v.y, v.z, value);
	}

	bool Entry_Exists (const int i, const int j, const int k) const
	{
		for (int p = (*table_of_lists) (Hash (i, j, k)); p != 0; p = (*links) (p)) if ( (*indices) (1, p) == i && (*indices) (2, p) == j && (*indices) (3, p) == k) return true;

		return false;
	}

	bool Entry_Exists (const VECTOR_3D<int>& v) const
	{
		return Entry_Exists (v.x, v.y, v.z);
	}

	bool Get_Entry (const int i, const int j, const int k, T& value) const
	{
		for (int p = (*table_of_lists) (Hash (i, j, k)); p != 0; p = (*links) (p)) if ( (*indices) (1, p) == i && (*indices) (2, p) == j && (*indices) (3, p) == k)
			{
				value = (*data) (p);
				return true;
			}

		return false;
	}

	bool Get_Entry (const VECTOR_3D<int>& v, T& value) const
	{
		return Get_Entry (v.x, v.y, v.z, value);
	}

	bool Change_Entry (const int i, const int j, const int k, const T& value) // if (i,j,k) already exists, sets its value
	{
		for (int p = (*table_of_lists) (Hash (i, j, k)); p != 0; p = (*links) (p)) if ( (*indices) (1, p) == i && (*indices) (2, p) == j && (*indices) (3, p) == k)
			{
				(*data) (p) = value;
				return true;
			}

		return false;
	}

	bool Change_Entry (const VECTOR_3D<int>& v, const T& value)
	{
		return Change_Entry (v.x, v.y, v.z, value);
	}

	void Delete_Entry (const int i, const int j, const int k)
	{
		for (int *p = & (*table_of_lists) (Hash (i, j, k)); (*p) != 0; p = & (*links) (*p)) if ( (*indices) (1, *p) == i && (*indices) (2, *p) == j && (*indices) (3, *p) == k)
			{
				int doomed_entry = *p;
				*p = (*links) (doomed_entry); // delete it from the list
				(*links) (doomed_entry) = free_list;
				free_list = doomed_entry; // prepend to the beginning of the free space list
				number_of_entries--;
				return;
			}
	}

	void Delete_Entry (const VECTOR_3D<int>& index)
	{
		Delete_Entry (index.x, index.y, index.z);
	}

	void Delete_All_Entries() // doesn't update indices or data
	{
		ARRAY<int>::copy (0, *table_of_lists);
		(*links) (links->m) = 0;

		for (int i = 1; i <= links->m - 1; i++) (*links) (i) = i + 1;

		free_list = 1;
		number_of_entries = 0;
	}

	void Print_All_Keys()
	{
		for (int h = 1; h <= table_of_lists->m; h++) for (int p = (*table_of_lists) (h); p != 0; p = (*links) (p)) std::cout << (*indices) (1, p) << "," << (*indices) (2, p) << "," << (*indices) (3, p) << std::endl;
	}

	void Apply_Function_To_All_Entries (void (*function) (int, int, int, T&))
	{
		for (int h = 1; h <= table_of_lists->m; h++) for (int p = (*table_of_lists) (h); p != 0; p = (*links) (p)) function ( (*indices) (1, p), (*indices) (2, p), (*indices) (3, p), (*data) (p));
	}

	void Delete_Pointers_Stored_In_Table() // of course, only valid if pointers are stored in table
	{
		for (int h = 1; h <= table_of_lists->m; h++) for (int p = (*table_of_lists) (h); p != 0; p = (*links) (p))
			{
				delete (*data) (p);
				(*data) (p) = 0;
			}
	}

	void Reset_List_Arrays_Stored_In_Table() // of course, only works if pointers to LIST_ARRAY are stored in table
	{
		for (int h = 1; h <= table_of_lists->m; h++) for (int p = (*table_of_lists) (h); p != 0; p = (*links) (p)) (*data) (p)->Reset_Current_Size_To_Zero();
	}

//#####################################################################
};
}
#endif
