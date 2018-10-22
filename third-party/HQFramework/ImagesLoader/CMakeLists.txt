include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/zlib-1.2.6)

set (MAIN_SRC_FILES 
	3STC_DXT.cpp Bitmap.cpp BMP.cpp DDS.cpp TGA.cpp ETC.cpp ImgByteStream.cpp JPEG.cpp KTX.cpp PNGImg.cpp PVR.cpp
)

set (JPEG_LIB_SRC_FILES
jpeg-8d/jaricom.c jpeg-8d/jcomapi.c jpeg-8d/jdapimin.c jpeg-8d/jdapistd.c jpeg-8d/jdarith.c jpeg-8d/jdatadst.c jpeg-8d/jdatasrc.c jpeg-8d/jdcoefct.c jpeg-8d/jdcolor.c jpeg-8d/jddctmgr.c jpeg-8d/jdhuff.c jpeg-8d/jdinput.c jpeg-8d/jdmainct.c jpeg-8d/jdmarker.c jpeg-8d/jdmaster.c jpeg-8d/jdmerge.c jpeg-8d/jdpostct.c jpeg-8d/jdsample.c jpeg-8d/jdtrans.c jpeg-8d/jerror.c jpeg-8d/jidctflt.c jpeg-8d/jidctfst.c jpeg-8d/jidctint.c jpeg-8d/jmemansi.c jpeg-8d/jmemmgr.c jpeg-8d/jquant1.c jpeg-8d/jquant2.c jpeg-8d/jutils.c )


set (PNG_LIB_SRC_FILES
lpng159/png.c lpng159/pngerror.c lpng159/pngget.c lpng159/pngmem.c lpng159/pngpread.c lpng159/pngread.c lpng159/pngrio.c lpng159/pngrtran.c lpng159/pngrutil.c lpng159/pngset.c lpng159/pngtrans.c )

set (ZLIB_SRC_FILES
zlib-1.2.6/adler32.c zlib-1.2.6/compress.c zlib-1.2.6/crc32.c zlib-1.2.6/deflate.c zlib-1.2.6/gzclose.c zlib-1.2.6/gzlib.c zlib-1.2.6/gzread.c zlib-1.2.6/gzwrite.c zlib-1.2.6/infback.c zlib-1.2.6/inffast.c zlib-1.2.6/inflate.c zlib-1.2.6/inftrees.c zlib-1.2.6/trees.c zlib-1.2.6/uncompr.c zlib-1.2.6/zutil.c )

add_library(ImagesLoader

	STATIC

	${MAIN_SRC_FILES}
	${JPEG_LIB_SRC_FILES}
	${PNG_LIB_SRC_FILES}
	)


find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
	# exclude zlib sources
else()
	target_sources(ImagesLoader PRIVATE ${ZLIB_SRC_FILES})
endif(ZLIB_FOUND)

target_include_directories(
            ImagesLoader

            PRIVATE

            ..
            )
