import sys
import os
import subprocess
import shutil

def print_error(error):
    print("\x1B[0;49;91m==>\x1B[0m \x1B[1m{}…\x1B[0m".format(error))

def print_usage():
    print("Usage: python3 {} appDir [options]\nOptions:\n -b, makes a backup of the app\n -c, makes a copy of the app and modifies it instead of the original app\n -q, quiets the output\n -r, reverses the architecture to remove (for testing purposes or to send a thinned app to someone else)\n -p, only prints list of files that can be thinned (lipo is not called)".format(sys.argv[0]))
    exit()

if len(sys.argv) > 1:
    if not os.path.isdir(sys.argv[1]):
        print_error("Invalid Directory")
        print_usage()
    elif not os.access(sys.argv[1], os.W_OK | os.X_OK):
        print_error("Directory not writeable. Copy it to another folder where it can be written to")
        exit()
else:
    print_usage()

appDir = sys.argv[1]
quiet = False
hasUB = False
printOnly = False
remA64 = True
arch = "arm64"
if os.uname().machine == arch:
    remA64 = False

def thin_machO(archName,filePath):
    global hasUB
    if not hasUB:
        if not quiet:
            if printOnly:
                print("\x1B[0;49;94m==>\x1B[0m \x1B[1mThe following Mach-O files can be thinned:\x1B[0m")
            else:
                print("\x1B[0;49;94m==>\x1B[0m \x1B[1mThinned the following Mach-O files:\x1B[0m")
        hasUB = True
    if not printOnly:
        subprocess.Popen("lipo -remove {} \"{}\" -o tempFile".format(archName,filePath), shell=True).communicate()
        shutil.move("tempFile",filePath)
    if not quiet:
        print("\x1B[0;49;92m >\x1B[0m {} [{}]".format(filePath.removeprefix(appDir),archName))

def recur_check(dir):
    for filename in os.listdir(dir):
        filename = os.path.join(dir,filename)
        if os.path.islink(filename):
            continue
        elif os.path.isdir(filename):
            recur_check(filename)
        elif os.path.isfile(filename):
            with open(filename, "rb") as bin:
                if bin.read(4) == b"\xca\xfe\xba\xbe" and bin.read(3) == b"\x00\x00\x00":
                    numArchs = bin.read(1)[0]
                    if numArchs > 1:
                        has_x86 = False
                        has_arm64 = False
                        has_arm64e = False
                        for i in range(0,numArchs):
                            cpu = bin.read(8)
                            if cpu == b"\x01\x00\x00\x07\x00\x00\x00\x03":
                                has_x86 = True
                            elif cpu == b"\x01\x00\x00\x0c\x00\x00\x00\x00":
                                has_arm64 = True   
                            elif cpu == b"\x01\x00\x00\x0c\x80\x00\x00\x02":
                                has_arm64e = True   
                            bin.seek(28 + i * 20)
                        if has_x86 and has_arm64 and remA64:
                            thin_machO("arm64",filename)
                        if has_x86 and has_arm64e and remA64:
                            thin_machO("arm64e",filename)
                        if (has_arm64 or has_arm64e) and has_x86 and not remA64:
                            thin_machO("x86_64",filename)

for x in range (2,len(sys.argv)):
    arg = sys.argv[x]
    if arg == "-b":
        if not os.access(os.path.dirname(appDir), os.W_OK | os.X_OK):
            print_error("Cannot make backup, parent directory not writeable")
            exit()
        else:
            print("\x1B[0;49;35m==>\x1B[0m \x1B[1mMaking Backup…\x1B[0m")
            split = os.path.splitext(appDir)
            backupDir = split[0] + "_Backup" + split[1]
            if os.path.exists(backupDir):
                shutil.rmtree(backupDir)
            shutil.copytree(sys.argv[1],backupDir,symlinks=True)
    elif arg == "-c":
        if not os.access(os.path.dirname(appDir), os.W_OK | os.X_OK):
            print_error("Cannot make copy, parent directory not writeable")
            exit()
        else:
            print("\x1B[0;49;35m==>\x1B[0m \x1B[1mMaking and thinning copy…\x1B[0m")
            split = os.path.splitext(appDir)
            appDir = split[0] + "_ThinnedCopy" + split[1]
            if os.path.exists(appDir):
                shutil.rmtree(appDir)
            shutil.copytree(sys.argv[1],appDir,symlinks=True)
    elif arg == "-q":
        quiet = True
    elif arg == "-r":
        remA64 ^= 1
    elif arg == "-p":
        printOnly = True
    else:
        print_usage()

if not remA64:
    arch="x86_64"
print("\x1B[0;49;34m==>\x1B[0m \x1B[1mRemoving {} portions…\x1B[0m".format(arch))

recur_check(appDir)

if not hasUB:
    print("\x1B[0;49;94m==>\x1B[0m \x1B[1mNothing to thin…\x1B[0m")