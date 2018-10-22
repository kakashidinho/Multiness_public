////////////////////////////////////////////////////////////////////////////////////////
//
// Multiness - NES/Famicom emulator written in C++
// Based on Nestopia emulator
//
// Copyright (C) 2016-2018 Le Hoang Quyen
//
// This file is part of Multiness.
//
// Multiness is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Multiness is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Multiness; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

package com.hqgame.networknes;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;

/**
 * Created by le on 6/12/2016.
 */
public class UniqueArray<T> {
    private HashMap<T, Integer> existingTable = new HashMap<>();
    private ArrayList<T> items = new ArrayList<>();

    public boolean add(T obj) {
        int currentSize = size();
        if (existingTable.containsKey(obj) || !items.add(obj))
            return false;

        existingTable.put(obj, currentSize);

        return true;
    }

    public void addOrUpdate(T obj) {
        int idx = getIndex(obj);
        if (idx < 0)
            add(obj);
        else
            items.set(idx, obj);
    }

    public boolean contains(T obj) {
        return existingTable.containsKey(obj);
    }

    public T get(int index) throws ArrayIndexOutOfBoundsException {
        return items.get(index);
    }

    public int getIndex(T obj) {
        Integer idx = existingTable.get(obj);
        if (idx == null)
            return -1;
        return idx;
    }

    public T remove(int index) throws ArrayIndexOutOfBoundsException {
        T obj = items.get(index);
        existingTable.remove(obj);
        items.remove(index);

        return obj;
    }

    public void clear() {
        items.clear();
        existingTable.clear();
    }

    public int size() {
        return items.size();
    }
}
