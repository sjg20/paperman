#ifndef TEST_FILE_H
#define TEST_FILE_H

#include <QObject>

#include "suite.h"

class TestFile: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   //! typeFromName maps extensions (and unknown extensions) to e_type
   void testTypeFromName();

   //! typeName/typeExt round-trip and stay aligned with e_type
   void testTypeNameAndExt();

   //! envFromName/envToName round-trip; unknown name returns Env_count
   void testEnvNames();

   //! decodePageNumber/encodePageNumber round-trip for the _pN convention
   void testPageNumberCodec();

   //! extMatchesType compares the file's own type against the supplied ext
   void testExtMatchesType();

   //! not_impl returns a non-null err with a stable code
   void testNotImpl();

   //! createFile dispatches each e_type to the right subclass and sets
   //! basename/leaf/ext/pathname consistently
   void testCreateFileDispatch();

   //! Loading and pulling basic metadata from each real fixture
   void testFixtureMetadata();

   //! encodeFile/decodeFile round-trips position + sizes through a string
   void testEncodeDecode();

   //! getImage on Filemax/Filepdf/Filejpeg returns a populated image with
   //! plausible size and bpp; Fileother returns not_impl
   void testGetImage();

   //! getPreviewPixmap returns a non-null pixmap smaller than the full image
   void testGetPreviewPixmap();

   //! setThumbnail makes Fileother::pixmap return the injected image rather
   //! than the generic unknown placeholder
   void testFileotherSetThumbnail();

   //! Fileother stubs out every mutation with not_impl, and duplicate()
   //! signals 'not supported' without erroring
   void testFileotherMutationsNotImpl();

   //! supportsJpeg + the base addPageJpeg fallback reflect each type's
   //! actual capability: only Filepdf advertises JPEG support
   void testSupportsJpeg();

   //! stackItem rejects stacking files of different types before dispatching
   //! to the per-subclass stackStack
   void testStackItemTypeMismatch();

   //! getword/gethw return real file bytes even when the read straddles
   //! the end of a cache window
   void testCacheBoundaryReads();

   //! transformPage rotates and mirrors pages of each file type
   void testTransformPage();

   //! PDF pages render correctly after rotation, mono and colour
   void testPdfMonoRender();

   //! a re-rendered transformed page matches the reference image
   //! transformed in memory
   void testTransformMatchesReference();

   //! transforming an empty stack reports an error and leaves the
   //! file intact
   void testTransformEmptyStack();

   //! a rotated page is cached for display and stays consistent with
   //! the file
   void testTransformImageCache();

   //! jpeg annotations round-trip through exiftool, including the
   //! multi-line ocr text
   void testJpegAnnotations();

   //! transformPage() on a jpeg rewrites the image on disk
   void testJpegTransform();

   //! removePages then restorePages round-trips the page count without
   //! crashing (restorePages must open the file before reading chunks)
   void testRemoveRestorePages();

private:
   //! Copy a file from test/files into destDir; returns full path
   QString copyFixture(const QString &name, const QString &destDir);
};

#endif // TEST_FILE_H
