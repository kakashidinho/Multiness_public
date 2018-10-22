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

import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.view.SurfaceHolder;

import java.io.InvalidObjectException;
import java.util.StringTokenizer;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

/**
 * Created by lehoangquyen on 7/3/16.
 */
public class GLUtils {
    public static class ContextFactory implements GLSurfaceView.EGLContextFactory {
        private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

        private int mESVersionMajor = 2;
        private SurfaceHolder mSurfaceHolder;

        public ContextFactory(SurfaceHolder holder) {
            mSurfaceHolder = holder;
        }

        public int getESVersionMajor() {
            return mESVersionMajor;
        }

        public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) {
            System.out.println("trying to create OpenGL ES 3.0 context");
            mESVersionMajor = 3;

            EGLContext context = EGL10.EGL_NO_CONTEXT;

            //try to create ES 3 context
            if (eglConfig != null)
            {
                ConfigChooser configChooser = new ConfigChooser();
                int version_bits = configChooser.findConfigAttrib(egl, display, eglConfig, EGL10.EGL_RENDERABLE_TYPE, 0);
                if (0 != (version_bits & ConfigChooser.EGL_OPENGL_ES3_BIT))//try to create ES3 context
                    context = createContext(egl, display, eglConfig, 3);
            }

            if (context != EGL10.EGL_NO_CONTEXT && context != null)
            {
                //verify GL version
                if (!verifyContextES3(egl, display, context, eglConfig)) {
                    System.out.println( "OpenGL ES 3.0 context is invalid");

                    //invalidate this context
                    destroyContext(egl, display, context);
                    context = EGL10.EGL_NO_CONTEXT;
                }
            }

            //if (context != EGL10.EGL_NO_CONTEXT)
            if (context == EGL10.EGL_NO_CONTEXT)
            {
                System.out.println( "creating OpenGL ES 2.0 context");
                mESVersionMajor = 2;
                context = createContext(egl, display, eglConfig, 2);
            }

            return context;
        }

        public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) {
            egl.eglDestroyContext(display, context);
        }

        private EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig, int version)
        {
            checkEglError("Before eglCreateContext", egl);
            int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, version, EGL10.EGL_NONE };
            EGLContext context = egl.eglCreateContext(display, eglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
            checkEglError("After eglCreateContext", egl);
            return context;
        }

        private EGLSurface createTempSurface(EGL10 egl, EGLDisplay display, EGLConfig tempConfig) {
            if (tempConfig == null)
                return null;

            try {
                EGLSurface surface = egl.eglCreateWindowSurface(display, tempConfig, mSurfaceHolder, null);
                return surface;
            } catch (Exception e) {
                e.printStackTrace();
                return null;
            }
        }

        private boolean verifyContextES3(EGL10 egl, EGLDisplay display, EGLContext context, EGLConfig tempConfig) {
            try {
                //save old context
                /*
                EGLDisplay currentDisplay = egl.eglGetCurrentDisplay();
                EGLContext currentContext = egl.eglGetCurrentContext();
                EGLSurface currentDrawSurface = egl.eglGetCurrentSurface(EGL10.EGL_DRAW);
                EGLSurface currentReadSurface = egl.eglGetCurrentSurface(EGL10.EGL_READ);
                */
                //make context current
                EGLSurface tempSurface = createTempSurface(egl, display, tempConfig);
                if (tempSurface == null)
                    return false;

                egl.eglMakeCurrent(display, tempSurface, tempSurface, context);
                if (egl.eglGetError() != EGL10.EGL_SUCCESS)
                {
                    egl.eglDestroySurface(display, tempSurface);
                    return false;
                }

                //check version string
                String version = GLES20.glGetString(GLES20.GL_VERSION);

                egl.eglMakeCurrent(display, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);
                egl.eglDestroySurface(display, tempSurface);

                StringTokenizer st = new StringTokenizer(version);
                st.nextToken();//OpenGL
                st.nextToken();//ES

                //version
                String versionNumString = st.nextToken();

                double versionNum = Double.parseDouble(versionNumString);
                if (versionNum < 3)//not valid 3.0 context
                    throw new InvalidObjectException("invalid GL context version " + versionNumString);

                return true;
            } catch (Exception e) {
                e.printStackTrace();

                return false;
            }
        }
    }

    private static void checkEglError(String prompt, EGL10 egl) {
        int error;
        while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) {
            System.err.println(String.format("%s: EGL error: 0x%x", prompt, error));
        }
    }

    public static class ConfigChooser implements GLSurfaceView.EGLConfigChooser {

        public ConfigChooser() {
            this(4, 4, 4, 4, 0, 0);
        }

        public ConfigChooser(int r, int g, int b, int a, int depth, int stencil) {
            mRedSize = r;
            mGreenSize = g;
            mBlueSize = b;
            mAlphaSize = a;
            mDepthSize = depth;
            mStencilSize = stencil;
        }

        /* This EGL config specification is used to specify rendering.
         * We use a minimum size of 4 bits for red/green/blue, but will
         * perform actual matching in chooseConfig() below.
         */
        public static int EGL_OPENGL_ES2_BIT = 0x0004;
        public static int EGL_OPENGL_ES3_BIT = 0x0040;

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {

	        /* Get the number of minimally matching EGL configurations
	         */
            int[] configAttribs =
            {
                    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT,
                    EGL10.EGL_RED_SIZE, mRedSize,
                    EGL10.EGL_GREEN_SIZE, mGreenSize,
                    EGL10.EGL_BLUE_SIZE, mBlueSize,
                    EGL10.EGL_ALPHA_SIZE, mAlphaSize,
                    EGL10.EGL_DEPTH_SIZE, mDepthSize,
                    EGL10.EGL_STENCIL_SIZE, mStencilSize,
                    EGL10.EGL_NONE
            };

            int[] num_config = new int[1];

            egl.eglChooseConfig(display, configAttribs, null, 0, num_config);

            int numConfigs = num_config[0];
            if (numConfigs == 0)//try with ES2.0
            {
                configAttribs[1] = EGL_OPENGL_ES2_BIT;
                egl.eglChooseConfig(display, configAttribs, null, 0, num_config);
            }

            numConfigs = num_config[0];
            if (numConfigs <= 0) {
                throw new IllegalArgumentException("No configs match configSpec");
            }

	        /* Allocate then read the array of minimally matching EGL configs
	         */
            EGLConfig[] configs = new EGLConfig[numConfigs];
            egl.eglChooseConfig(display, configAttribs, configs, numConfigs, num_config);

            if (Settings.DEBUG) {
                printConfigs(egl, display, configs);
            }
	        /* Now return the "best" one
	         */
            return chooseConfig(egl, display, configs);
        }

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display,
                                      EGLConfig[] configs) {

            int minR = Integer.MAX_VALUE;
            int minG = Integer.MAX_VALUE;
            int minB = Integer.MAX_VALUE;
            int minA = Integer.MAX_VALUE;

            int bestSupportedRenderableTypes = 0;

            EGLConfig bestConfig = null;

            for(EGLConfig config : configs) {
                int d = findConfigAttrib(egl, display, config,
                        EGL10.EGL_DEPTH_SIZE, 0);
                int s = findConfigAttrib(egl, display, config,
                        EGL10.EGL_STENCIL_SIZE, 0);

                // We need at least mDepthSize and mStencilSize bits
                if (d < mDepthSize || s < mStencilSize)
                    continue;

                // We want an *exact* match for red/green/blue/alpha
                int r = findConfigAttrib(egl, display, config,
                        EGL10.EGL_RED_SIZE, 0);
                int g = findConfigAttrib(egl, display, config,
                        EGL10.EGL_GREEN_SIZE, 0);
                int b = findConfigAttrib(egl, display, config,
                        EGL10.EGL_BLUE_SIZE, 0);
                int a = findConfigAttrib(egl, display, config,
                        EGL10.EGL_ALPHA_SIZE, 0);

                int renderable_bits = findConfigAttrib(egl, display, config,
                        EGL10.EGL_RENDERABLE_TYPE, 0);

                if (r >= mRedSize && g >= mGreenSize && b >= mBlueSize && a >= mAlphaSize
                        && (r <= minR && g <= minG && b <= minB && a <= minA)
                        && (r < minR || g < minG || b < minB || a < minA)
                        && bestSupportedRenderableTypes <= renderable_bits)
                {
                    minR = r;
                    minG = g;
                    minB = b;
                    minA = a;

                    bestSupportedRenderableTypes = renderable_bits;

                    bestConfig = config;
                }
            }
            return bestConfig;
        }

        public int findConfigAttrib(EGL10 egl, EGLDisplay display,
                                     EGLConfig config, int attribute, int defaultValue) {

            if (egl.eglGetConfigAttrib(display, config, attribute, mValue)) {
                return mValue[0];
            }
            return defaultValue;
        }

        private void printConfigs(EGL10 egl, EGLDisplay display,
                                  EGLConfig[] configs) {
            int numConfigs = configs.length;
            System.out.println(String.format("%d configurations", numConfigs));
            for (int i = 0; i < numConfigs; i++) {
                System.out.println(String.format("Configuration %d:\n", i));
                printConfig(egl, display, configs[i]);
            }
        }

        private void printConfig(EGL10 egl, EGLDisplay display,
                                 EGLConfig config) {
            int[] attributes = {
                    EGL10.EGL_BUFFER_SIZE,
                    EGL10.EGL_ALPHA_SIZE,
                    EGL10.EGL_BLUE_SIZE,
                    EGL10.EGL_GREEN_SIZE,
                    EGL10.EGL_RED_SIZE,
                    EGL10.EGL_DEPTH_SIZE,
                    EGL10.EGL_STENCIL_SIZE,
                    EGL10.EGL_CONFIG_CAVEAT,
                    EGL10.EGL_CONFIG_ID,
                    EGL10.EGL_LEVEL,
                    EGL10.EGL_MAX_PBUFFER_HEIGHT,
                    EGL10.EGL_MAX_PBUFFER_PIXELS,
                    EGL10.EGL_MAX_PBUFFER_WIDTH,
                    EGL10.EGL_NATIVE_RENDERABLE,
                    EGL10.EGL_NATIVE_VISUAL_ID,
                    EGL10.EGL_NATIVE_VISUAL_TYPE,
                    0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
                    EGL10.EGL_SAMPLES,
                    EGL10.EGL_SAMPLE_BUFFERS,
                    EGL10.EGL_SURFACE_TYPE,
                    EGL10.EGL_TRANSPARENT_TYPE,
                    EGL10.EGL_TRANSPARENT_RED_VALUE,
                    EGL10.EGL_TRANSPARENT_GREEN_VALUE,
                    EGL10.EGL_TRANSPARENT_BLUE_VALUE,
                    0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
                    0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
                    0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
                    0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
                    EGL10.EGL_LUMINANCE_SIZE,
                    EGL10.EGL_ALPHA_MASK_SIZE,
                    EGL10.EGL_COLOR_BUFFER_TYPE,
                    EGL10.EGL_RENDERABLE_TYPE,
                    0x3042 // EGL10.EGL_CONFORMANT
            };
            String[] names = {
                    "EGL_BUFFER_SIZE",
                    "EGL_ALPHA_SIZE",
                    "EGL_BLUE_SIZE",
                    "EGL_GREEN_SIZE",
                    "EGL_RED_SIZE",
                    "EGL_DEPTH_SIZE",
                    "EGL_STENCIL_SIZE",
                    "EGL_CONFIG_CAVEAT",
                    "EGL_CONFIG_ID",
                    "EGL_LEVEL",
                    "EGL_MAX_PBUFFER_HEIGHT",
                    "EGL_MAX_PBUFFER_PIXELS",
                    "EGL_MAX_PBUFFER_WIDTH",
                    "EGL_NATIVE_RENDERABLE",
                    "EGL_NATIVE_VISUAL_ID",
                    "EGL_NATIVE_VISUAL_TYPE",
                    "EGL_PRESERVED_RESOURCES",
                    "EGL_SAMPLES",
                    "EGL_SAMPLE_BUFFERS",
                    "EGL_SURFACE_TYPE",
                    "EGL_TRANSPARENT_TYPE",
                    "EGL_TRANSPARENT_RED_VALUE",
                    "EGL_TRANSPARENT_GREEN_VALUE",
                    "EGL_TRANSPARENT_BLUE_VALUE",
                    "EGL_BIND_TO_TEXTURE_RGB",
                    "EGL_BIND_TO_TEXTURE_RGBA",
                    "EGL_MIN_SWAP_INTERVAL",
                    "EGL_MAX_SWAP_INTERVAL",
                    "EGL_LUMINANCE_SIZE",
                    "EGL_ALPHA_MASK_SIZE",
                    "EGL_COLOR_BUFFER_TYPE",
                    "EGL_RENDERABLE_TYPE",
                    "EGL_CONFORMANT"
            };
            int[] value = new int[1];
            for (int i = 0; i < attributes.length; i++) {
                int attribute = attributes[i];
                String name = names[i];
                if ( egl.eglGetConfigAttrib(display, config, attribute, value)) {
                    System.out.println(String.format("  %s: %d\n", name, value[0]));
                } else {
                    // Log.w(TAG, String.format("  %s: failed\n", name));
                    while (egl.eglGetError() != EGL10.EGL_SUCCESS);
                }
            }
        }

        // Subclasses can adjust these values:
        protected int mRedSize;
        protected int mGreenSize;
        protected int mBlueSize;
        protected int mAlphaSize;
        protected int mDepthSize;
        protected int mStencilSize;
        private int[] mValue = new int[1];
    }
}
