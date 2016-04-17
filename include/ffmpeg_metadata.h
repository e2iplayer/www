#ifndef _ffmpeg_metadata_123
#define _ffmpeg_metadata_123

/* these file contains a list of metadata tags which can be used by applications
 * to stream specific information. it maps the tags to ffmpeg specific tags.
 *
 * fixme: if we add other container for some resons later (maybe some other libs
 * support better demuxing or something like this), then we should think on a
 * more generic mechanism!
 */

/* metatdata map list:
 */
char* metadata_map[] =
{
 /* our tags      ffmpeg tag / id3v2 */  
   "Title",       "TIT2",
   "Title",       "TT2",
   "Artist",      "TPE1",
   "Artist",      "TP1",
   "AlbumArtist", "TPE2",
   "AlbumArtist", "TP2",
   "Album",       "TALB",
   "Album",       "TAL",
   "Year",        "TDRL",  /* fixme */
   "Year",        "TDRC",  /* fixme */
   "Comment",     "unknown",
   "Track",       "TRCK",
   "Track",       "TRK",
   "Copyright",   "TCOP",
   "Composer",    "TCOM",
   "Genre",       "TCON",
   "Genre",       "TCO",   
   "EncodedBy",   "TENC",   
   "EncodedBy",   "TEN", 
   "Language",    "TLAN",
   "Performer",   "TPE3",
   "Performer",   "TP3",
   "Publisher",   "TPUB",
   "Encoder",     "TSSE",
   "Disc",        "TPOS",
   NULL
};

#endif
