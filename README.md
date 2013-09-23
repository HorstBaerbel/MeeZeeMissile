Miezi Missile
========

Use opencv to control a M&S or Dream Cheeky foam missile launcher to shoot when detecting movement. Runs on the command line and only works on Linux with super user rights atm. Go cry somewhere else or tell me how to work around it...

License
========

[BSD-2-Clause](http://opensource.org/licenses/BSD-2-Clause), see LICENSE.md
The Findlibusb-1.0.cmake script is from [here](http://browse-code.mrpt.org/mrpt-0.9.4/cmakemodules/Findlibusb-1.0.cmake). Thanks Andreas!

Building
========

**Use CMake:**
<pre>
cd src
cmake .
make
</pre>

GCC 4.7 is needed to compile. You also need OpenCV. Install it with:
```
sudo apt-get libopencv-dev
```

You might also need libusb-1.0. Try installing the development package with:
```
sudo apt-get libusb-1.0-dev
```

I found a bug or have suggestion
========

The best way to report a bug or suggest something is to post an issue on GitHub. Try to make it simple. You can also contact me via email if you want to.