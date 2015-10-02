

== Building Xonotic on OSX ==

This program builds on OSX with a few modifications.

o The OSX header for dispatch/object has Objective C code in it that
  GCC does not understand.  By patching the header, it will be
  compatible with both.

--- /usr/include/dispatch/object.h~     2014-09-09 13:53:42.000000000 -0700
+++ /usr/include/dispatch/object.h      2015-10-02 08:09:41.000000000 -0700
@@ -140,7 +140,11 @@
  * Instead, the block literal must be copied to the heap with the Block_copy()
  * function or by sending it a -[copy] message.
  */
+#ifdef __clang__
 typedef void (^dispatch_block_t)(void);
+#else
+typedef void *dispatch_block_t;
+#endif
 
 __BEGIN_DECLS

o Clone the xonotic repository.

  $ git clone https://github.com/xonotic/xonotic.git

o Fetch the dependencies.  This will download subprojects and tar files.

  $ cd xonotic
  $ ./all update

o One of the source files in darkplaces needs a prototype for ANSI
  compliance.

diff --git i/vid_sdl.c w/vid_sdl.c
index 942cbdd..0b2397a 100644
--- i/vid_sdl.c
+++ w/vid_sdl.c
@@ -34,6 +34,7 @@ Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 static cvar_t apple_mouse_noaccel = {CVAR_SAVE, "apple_mouse_noaccel", "1", "disables mouse acceleration while DarkPlaces is active"};
 static qboolean vid_usingnoaccel;
 static double originalMouseSpeed = -1.0;
+io_connect_t IN_GetIOHandle (void);
 io_connect_t IN_GetIOHandle(void)
 {
        io_connect_t iohandle = MACH_PORT_NULL;


o Install GCC using homebrew.  You should get version 4.9 at a
  minimum.

  $ brew install gcc

o Make homebrew the default.  This might require some tinkering.  If
  /usr/local/bin preceeds /usr/bin in your path, you can do something
  like this:

  $ ln -s gcc-4.9 /usr/local/bin/gcc

o Build

  $ ./all compile

o Run to verify that it works

  $ ./all run

o Add the haptics code to darkplaces

  $ cd darkplaces
  $ git remote add haptics git@github.com:ehrenbrav/darkplaces.git
  $ git fetch haptics haptics
  $ git checkout haptics

o Copy the library to a place where the build can find it.

o Rebuild

  $ ./all compile

o Update the resources so that we can see the haptics settings

  $ cd xonotic/data/xonotic-data.pk3dir
  $ git remote add haptics git@github.com:ehrenbrav/xonotic-data.pk3dir.git
  $ git fetch haptics haptics
  $ git checkout haptics

