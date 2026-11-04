Early, under development port of Micropolis to GNU/Linux using SDL3 for the frontend.

Originally based off the 1989 Unix source code, refactored from 1989 C to 2011 C language.

Feature parity with the 1989 Unix port has been audited: map views, tools, overlays, disasters, speeds, scenarios, options, and the City, Budget, and Graphs windows are all wired up. Cities can be saved and loaded from the File dialog.

Notes:
-Chalk and Eraser are no-op tools in the Unix port too, so they are not implemented here.
-The Dynamic Filter (Frob-O-Matic) and pie menus are intentionally out of scope.

TODO:
-Go on two bug hunts, one to correct modern era bugs, the other to correct bugs on the 1989 bugs file that were never fixed.
-Decide whether the Park palette label ($20) or the Park cost ($10) is correct.

![Alt text](shot.png)