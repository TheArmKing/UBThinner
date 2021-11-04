# UBThinner
A Script to thin Universal Apps on macOS quickly. It traverses through the given folder recursively, identifies any universal binaries and thins them using `lipo`. The default arch that is thinned is the opposite of your system's. Example - if you are on any `M1` device, then your system arch is `arm64`, so `x86_64` portions will be removed from every universal binary within the given folder/app.

# Usage
```
Usage: UBThinner[.sh/.py] appDir [options]
Options:
 -b, makes a backup of the app
 -c, makes a copy of the app and modifies it instead of the original app
 -q, quiets the output
 -r, reverses the architecture to remove (for testing purposes or to send a thinned app to someone else)
 -p, only prints list of files that can be thinned (lipo is not called)
```

# Notes 
- You will probably need the XCode Command Line Tools for `lipo`, install them in terminal by running `xcode-select --install`
- The `.cpp` file can be compiled using `g++`, which also needs the command line tools. After installing them, run the following `g++ -std=c++17 UBThinner.cpp -o UBThinner`
- If you get the `Directory not writeable. Copy it to another folder where it can be written to` error than you can either copy the app elsewhere (to the `Documents` folder for example) and run `UBThinner` there or you can run it as `root`. (su -> enter password -> run UBThinner)
- I first made it in `bash`, which is why comments are included only in the `.sh` file. Then I ported the code to `python`. Lastly, I thought of trying it out in `C++` to see how the speeds would compare. I used `std::filesystem` from `C++17` to remain sane. 
- The `bash` script is quite obviously the slowest. There is not much difference between the `python` and `cpp` scripts on my machine (M1 Air 8/256 GB)

