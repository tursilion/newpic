NewPic by M.Brent (Tursi)
-------------------------

This program scans a folder for images, resizes the image, converts it to BMP, then sets your 
desktop backdrop to it (optionally, see below). The latest version can generate a mosiac of images
from your drive, piecing together a more complete backdrop (especially useful for multi-monitor).

Download the latest release zip here: [https://github.com/tursilion/newpic/raw/master/dist/newpic.zip](https://github.com/tursilion/newpic/raw/master/dist/newpic.zip)

Basic use:

NEWPIC <optional argument=value pairs>

path=<path to pictures>
 Specify the path which will be recursively scanned for image files. Default is the working folder.
 This argument may be repeated to scan multiple paths.

map=<path to map file>
 Mainly intended for mosaics - will output a map of the mosaic with one line per image. If the input 
 image is a PNG file, it will be parsed and, if present, Image Magick virtual page information will be 
 extracted (otherwise the last four values will always be zero). In the event that 'checkflip' is set,
 flipped files will be indicated with negative width, height, or both. Each line is pipe delimited and 
 looks like this:
  
  filepath|x|y|width|height|vpage_width|vpage_height|vpage_x_offset|vpage_y_offset

outwidth=<output width>
 Specify the width of the output image. Default is the width of the primary monitor.

outheight=<output height>
 Specify the height of the output image. Default is the height of the primary monitor.

outfile=<output filename>
 Specify the output filename. Default is C:\WINDOWS\BACKDROP.BMP (or WINNT if your Windows folder is there)

fillbuf=<output filename>
 For debugging purposes, outputs the fill buffer as a BMP file (black and white)

maxscale=<maximum permitted scale to fit - smaller images are discarded>
 This is an alternate way to specify a minimum filesize - and works better for mosaic since
 it allows very small files to be chosen for small openings (at random). Normally you'd
 probably want 4 or 5, but it depends on your image collection. Default is any.

minscale=<minimum permitted scale to fit - larger images are discarded>
 This is the opposite of maxscale, and requires that images would be scaled a minimum
 amount to be accepted. There are very few instances this is useful, and you need to have
 small source images for it to work.

minwidth=<minimum input width>
 Specify a minimum width for input files. Default is any.

minheight=<minimum input height>
 Specify a minimum height for input files. Default is any.

minmosaicx=<minimum space to fit mosaic width (default 160)>
 Specifies how small a block remaining is too small to put another image into.

minmosaicy=<minimum space to fit mosaic height (default 120)>
 Specifies how small a block remaining is too small to put another image into.

maxmosaicx=<maximum width of one mosaic image>
 When mosaic is enabled, limits the maximum width of any individual image to this

maxmosaicy=<minimum height of one mosaic image>
 When mosaic is enabled, limits the maximum height of any individual image to this

maxerr=<maximum errors per file attempt (default 6)>
 For each file, this specifies how many retries are permitted before giving up. Errors include
 not only file errors and unreadable images, but also images being rejected for size, color, scale,
 etc. Therefore with some collections it may be worth increasing from the default of 6.
 
hwnd=<window handle to render to, in decimal>
 For a mosaic, this will also draw each image to the specified window handle
 
filedelay=<time in milliseconds between each file in a mosaic (max 10000)>
 For a mosaic, how long to delay between processing each image (default is 0ms)

The follow commands set behavior by being present, and do not take an argument! 

background
 The program will set the Windows backdrop to the output file.

color
 The program will reject pictures that do not appear to be color. (Very simple algorithm).

filename
 Draw the chosen filename at the bottom center of the output image. Ignored when output is less
 then 150 pixels wide.

alwaysfilename
 Same as filename, but even for very small images.

mosaic
 Enables mosaic mode. Selectively smaller images will be generated until the desktop is full.

skipblank
 Will scan each loaded image, and skip it if it is only a single color (ie: blank)

noscale
 Does not stretch images to fit the hole. This tends to cause more holes in the resulting image, as 
 the program will only make a loose effort to fit images in the holes. Images larger than available
 holes are skipped, and counted as errors, which may cause premature termination of the search.
 
stretch
 Stretch image to fit, ignoring aspect ratio. In mosaic mode, this applies only to the last image. 
 This will override 'noscale'.

sequential
 In mosaic mode, only the first image is chosen randomly, the rest are chosen sequentially

biggestfirst
 In mosaic mode, add the largest images (x*y) first. If you have more than 50 images, you must include
 "YesIMeanIt", as this must open every image before starting in order to sort them, and this could be
 very slow on a large directory.

smallestfirst
 In mosaic mode, add the smallest images (x*y) first. If you have more than 50 images, you must include
 "YesIMeanIt", as this must open every image before starting in order to sort them, and this could be
 very slow on a large directory.

bigxfirst
 In mosaic mode, add the widest images (x) first. If you have more than 50 images, you must include
 "YesIMeanIt", as this must open every image before starting in order to sort them, and this could be
 very slow on a large directory.

bigyfirst
 In mosaic mode, add the tallest images (y) first. If you have more than 50 images, you must include
 "YesIMeanIt", as this must open every image before starting in order to sort them, and this could be
 very slow on a large directory.

smallxfirst
 In mosaic mode, add the narrowest images (x) first. If you have more than 50 images, you must include
 "YesIMeanIt", as this must open every image before starting in order to sort them, and this could be
 very slow on a large directory.

smallyfirst
 In mosaic mode, add the shortest images (y) first. If you have more than 50 images, you must include
 "YesIMeanIt", as this must open every image before starting in order to sort them, and this could be
 very slow on a large directory.

firstfile
 Sort the file list alphabetically, and start with the first one in the list. Most useful with 'sequential'

nodupes
 In mosaic mode, search for duplicate images and don't draw them. If you are generating an imagemap,
 the duplicate will still be logged in the map, but pointing at the location of the original data.

checkflip
 Like nodupes, this skips duplicated images in mosaic mode. However, it will only check for flipped images,
 checking horizontal, vertical, and both. (You need both nodupes and checkflip to check all possibilities).
 This may be slow. If generating an image map, flipped matches are indicated with negative width, height,
 or both.

randomfill
 In mosaic mode, choose the 'next' rectangle randomly instead of always selecting the largest one.

randomsize
 In mosaic mode, randomly size the subimages instead of trying for best fit

force43
 Assume picture is intended for square pixels, and adjust the stretch if your monitor is not. For
 instance, 1280x1024 has non-square pixels on a 4:3 display, while 800x600 does. This will skew
 the output size some and may not be ideal for all users. This adjustment will still take effect when 
 'noscale' is active.
 
bgfirst
 Sets the background color (ie: any holes) to match the first pixel of the first image chosen
 
pinkalpha
 If reading PNG files with an alpha channel, any alpha less than 0.5 will be remapped to a hot
 pink/magenta (RGB #FF00FF). This parameter will also set the background color to the same
 hot pink, but may be overridden by putting 'bgfirst' after it on the command line (also, this
 parameter will override 'bgfirst' if it comes after.)
 
stoponerr
 If an error occurs reading a file, writing the output, or if mosaic is in use and it fails to
 use all available files, the program will stop and wait for you to press Enter before proceeding.

HQuadrants=(string)
VQuadrants=(string)
 Divides the screen (Horizontally or vertically). Like HTML frames, the numbers represent fractions 
 of available space. So, '112' means to divide the screen into a total of 4 pieces. The first quadrant 
 uses 1/4, the second uses 1/4, and the third uses 2/4 (or 1/2). Each space may be optionally 
 subdivided on the perpendicular axis by using square brackets [] to define the space within, in this 
 case the [11] divides the third quadrant into two halves (1/2 and 1/2). HQuadrants divides the output 
 first Horizontally, VQuadrants divides the screen first vertically. They are mutually exclusive - if
 you define both, only one of them will be used. So, for example, HQuadrants=112[11] looks like:
 +---+---+------+
 |   |   |      |
 |   |   +------+
 |   |   |      |
 +---+---+------+
 
randomquads
 Processes the defined quadrants in a random order. 

returncount
 Errorlevel return will return the number of read images minus the number of failed images, instead of
 the normal codes (which aren't documented).
 
returnfailed
 Errorlevel return will return the number of failed images (otherwise 0).
 
monitors
 Automatically enumerates monitors on your system, and processes so that the output image aligns with
 them. This overrides outwidth and outheight, but all other options should work.
 
server
 This is an odd one, it runs newpic in a sort of server mode. Three named events are created:
 NEWPIC_EVENT_TRIGGER - set this event to run one full process (mosaic or not)
 NEWPIC_EVENT_COMPLETE - newpic sets this event when it is done
 NEWPIC_EVENT_TERMINATE - set this event to tell newpic to exit
 
 NewPic will not exit in this mode, when it runs out of images, it will reset the file list.
 Note that it will not rescan the disk!
 
This file is a Win32 command program, it requires Windows 95 or better to run. It probably
also requires Internet Explorer 4.0 or newer, but it may run on earlier. It's not tested on
anything earlier than Win98.

This program is now unicode specific, so it might not even run on pre-unicode systems. I'll see if I
can put a previous version into the git history...

Error codes:
These codes are displayed when stoponerr is set, they also are set as the program return
code for batch files:
10 - no input files were found
20 - could not open output file
30 - could not write to output file
99 - mosaic failed to use all files - out of space

Suggested uses:
-have Scheduled Tasks (or another scheduler) run the program every couple of hours to automatically 
change your backdrop. (I have it change when the system is idle, so I always come back to a new picture.)

-This is a tricky one. Have Scheduled Tasks call the program as above, but do not change the backdrop. 
Scale to an image smaller than your screen, and make the image into an Active Desktop component. 
(Active Desktop is enumerated when the program is called and any items representing the output file 
are refreshed.)

-A lot of changes have come in to allow the program to condense individual images into a texture
page via the mosaic function.

Sometimes the program can not load an image, due to the wide variety of weird headers. The program 
will automatically try again (with the next file), up to five times (or maxerr) before giving up. The 
program has a maximum of 500000 input files. 

Active Desktop component refresh only works with 8.3 filenames (at least under Win98, likely works 
better under 2k and XP).
