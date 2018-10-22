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

/**
 * Created by lehoangquyen on 6/6/16.
 */
public interface AsyncOperation {
    public void cancel();

    public static abstract class Runnable implements AsyncOperation, java.lang.Runnable {

        @Override
        final public synchronized void cancel() {
            mIsCanceled = true;

            cancelImpl();
        }

        @Override
        final public synchronized void run() {
            if (!mIsCanceled)
                runImpl();
        }

        protected abstract void runImpl();

        protected void cancelImpl() {}

        private volatile boolean mIsCanceled = false;
    }
}
