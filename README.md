grafikahf1
----------
## Specification ##
This is a homework for my university course "Számítógépes Grafika" (Computer Graphics).

The specification in hungarian:

> Twinkle, twinkle little star...
> 
> A 2D virtuális világ három, legalább 7-ágú forgó és pulzáló csillagot
> tartalmaz, amelyek színe különböző.A legfényesebb csillag az
> egérklikkek által kijelölt zárt, -0.8-as tenziójú Catmull-Rom spline-t
> követi (azaz egy kontrollpontban a sebesség az előző és következő
> szegmensek átlagsebességeinek a 0.9-de). A Catmull-Rom spline
> csomóértékei a gomblenyomáskori idők. A legutolsó és legelső
> kontrollpont között 0.5 sec telik el, majd a csillag periodikusan újra
> bejárja a görbét. A görbe mindenhol folytonosan deriválható, a legelső
> pontban is (azaz nem törik meg itt sem).
> 
> A másik két csillagot a legfényesebb csillag a Newton féle gravitációs
> erővel vonzza, azaz mozgatja, miközben sebességgel arányos surlódási
> erő fékezi őket. A gravitációs konstanst úgy kell megválasztani, hogy
> a mozgás élvezhető legyen, azaz a csillagok a képernyőn maradjanak.
> 
> SPACE lenyomására a virtuális kameránkat a fényes csillaghoz
> kapcsolhatjuk, egyébként a kamera statikus.

# Build

### Apple ###
For mac it should run in Xcode if the GLUT and OpenGL frameworks are added in the build settings.

### Windows ###
For windows you need to download the frameworks, then add them to your project in a lib folder.