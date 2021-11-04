#!/bin/bash
exec 6>&1 # saves stdout. Used for quiet option later. Thanks to https://stackoverflow.com/a/36003000

# prints given text with colored (red) output
print_error(){
    echo -en "\x1B[0;49;91m==>\x1B[0m \x1B[1m${1}…\x1B[0m\n"
}

# prints usage and quits
print_usage(){
    echo -en "Usage: $0 appDir [options]\nOptions:\n -b, makes a backup of the app\n -c, makes a copy of the app and modifies it instead of the original app\n -q, quiets the output\n -r, reverses the architecture to remove (for testing purposes or to send a thinned app to someone else)\n -p, only prints list of files that can be thinned (lipo is not called)\n"
    exit
}

# main function for thinning a mach-o file using lipo
thin_machO(){
    if [ "$hasUB" -eq 0 ]; then
        if [ "$printOnly" -eq 0 ]; then
            echo -en "\x1B[0;49;94m==>\x1B[0m \x1B[1mThinned the following Mach-O files:\x1B[0m\n"
        else
            echo -en "\x1B[0;49;94m==>\x1B[0m \x1B[1mThe following Mach-O files can be thinned:\x1B[0m\n"
        fi
        hasUB=1
    fi
    if [ "$printOnly" -eq 0 ]; then
        lipo -remove "$1" "$2" -o tempFile
        mv tempFile "$2"
    fi
    echo -en "\x1B[0;49;92m >\x1B[0m ${2#"$appDir"} [${1}]\n"
}

# main recursive function (loops through the main directory and calls itself on a sub-directory)
recur_check() {
    for filename in "${1}"/*; do
        if [ -L "$filename" ]; then # if it's a symlink then skip
            continue
        elif [ -d "$filename" ]; then # if it's a directory then call the recursive function on it
            recur_check "$filename"
        elif [ -f "$filename" ]; then
            first8b="$(xxd -p -l8 "$filename")" # if it's a file get the first 8 bytes
            if [ "${first8b:0:15}" == "cafebabe0000000" ]; then # following https://opensource.apple.com/source/file/file-47/file/magic/Magdir/cafebabe.auto.html
                numArchs="${first8b:15:1}"
                if [ "$numArchs" -gt 1 ] ; then
                    (( totalBytes = 8 + (numArchs - 1) * 20 )) # every arch's header is 20 bytes but we only need the first 8
                    header="$(xxd -p -s 8 -l${totalBytes} "$filename" | tr -d \\n)" # calling xxd once instead of twice/thrice seems to be faster
                    seek=0
                    has_x86=0
                    has_arm64=0
                    has_arm64e=0
                    for (( i=0; i<numArchs; i++ )); do
                        cpu="${header:seek:16}" # first 8 bytes of an arch's header identify the cpu type & subtype
                        if [ "$cpu" == "0100000700000003" ]; then
                            has_x86=1
                        elif [ "$cpu" == "0100000c00000000" ]; then
                            has_arm64=1
                        elif [ "$cpu" == "0100000c80000002" ]; then
                            has_arm64e=1
                        fi
                        (( seek = seek + 40 )) 
                    done
                    if [ $has_x86 -eq 1 ] && [ $has_arm64 -eq 1 ] && [ "$remA64" -eq 1 ] ; then thin_machO arm64 "$filename"; fi
                    if [ $has_x86 -eq 1 ] && [ $has_arm64e -eq 1 ] && [ "$remA64" -eq 1 ] ; then thin_machO arm64e "$filename"; fi
                    if [ $has_x86 -eq 1 ] && ( [ $has_arm64 -eq 1 ] || [ $has_arm64e -eq 1 ] ) && [ "$remA64" -eq 0 ] ; then thin_machO x86_64 "$filename"; fi
                fi
            fi
        fi
    done
}

# check if input dir is valid
if [ ! -d "$1" ]; then print_error "Invalid Directory" && print_usage; fi
if [ ! -w "$1" ]; then print_error "Directory not writeable. Copy it to another folder where it can be written to" && exit; fi

# variable initialisation
appDir="$1"
quiet=0
hasUB=0
printOnly=0
remA64=1
arch="arm64"
if [ "$(arch)" == "$arch" ]; then remA64=0; fi

# argument parsing
for arg in "${@:2}"; do
    if [ "$arg" == "-b" ]; then
        if [ ! -w "$(dirname "$appDir")" ]; then 
            print_error "Cannot make backup, parent directory not writeable" && exit
        else
            echo -en "\x1B[0;49;35m==>\x1B[0m \x1B[1mMaking Backup…\x1B[0m\n"
            backupDir="${1%.*}_Backup.${1##*.}"
            if [ -d "$backupDir" ]; then rm -rf "$backupDir"; fi
            cp -r "$1" "$backupDir"
        fi
    elif [ "$arg" == "-c" ]; then
        if [ ! -w "$(dirname "$appDir")" ]; then 
            print_error "Cannot make copy, parent directory not writeable" && exit
        else
            echo -en "\x1B[0;49;35m==>\x1B[0m \x1B[1mMaking and thinning copy…\x1B[0m\n"
            appDir="${1%.*}_ThinnedCopy.${1##*.}"
            if [ -d "$appDir" ]; then rm -rf "$appDir"; fi
            cp -r "$1" "$appDir"
        fi
    elif [ "$arg" == "-q" ]; then
        quiet=1
    elif [ "$arg" == "-r" ]; then
        (( remA64 = remA64 ^ 1 ))
    elif [ "$arg" == "-p" ]; then
        printOnly=1
    else
        print_usage
    fi
done

# redirects stdout to null (quiet)
if [ $quiet -eq 1 ]; then exec > /dev/null; fi

# printing after changes from quiet & reverse options have been made
if [ $remA64 -eq 0 ]; then arch="x86_64"; fi
echo -en "\x1B[0;49;34m==>\x1B[0m \x1B[1mRemoving ${arch} portions…\x1B[0m\n"

# starting check
recur_check "$appDir"

# prints message if there is nothing to thin
if [ $hasUB -eq 0 ]; then
    echo -en "\x1B[0;49;94m==>\x1B[0m \x1B[1mNothing to thin…\x1B[0m\n"
fi

exec 1>&6 6>&- # restores stdout