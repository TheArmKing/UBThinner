#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <sys/utsname.h>
#include <fstream>
namespace fs = std::filesystem;

const char* appDir;
const char* argv0;
bool quiet = false;
bool hasUB = false;
bool printOnly = false;
bool remA64 = true;
const char* arch = "arm64";

void print_error(const char* error){
    std::cout << "\x1B[0;49;91m==>\x1B[0m \x1B[1m" << error << "…\x1B[0m" << std::endl;
}

void print_usage(){
    std::cout << "Usage: " << argv0 << " appDir [options]\nOptions:\n -b, makes a backup of the app\n -c, makes a copy of the app and modifies it instead of the original app\n -q, quiets the output\n -r, reverses the architecture to remove (for testing purposes or to send a thinned app to someone else)\n -p, only prints list of files that can be thinned (lipo is not called)" << std::endl;
    exit(0);
}

void thin_machO(std::string archName, fs::path filePath){
    if (!hasUB){
        if (!quiet){
            if (printOnly){
                std::cout << "\x1B[0;49;94m==>\x1B[0m \x1B[1mThe following Mach-O files can be thinned:\x1B[0m" << std::endl;
            } else {
                std::cout << "\x1B[0;49;94m==>\x1B[0m \x1B[1mThinned the following Mach-O files:\x1B[0m" << std::endl;
            }
        }
        hasUB = true;
    }
    if(!printOnly){
        system(("lipo -remove " + archName + " \"" + filePath.string() + "\" -o tempFile").c_str());
        fs::rename(fs::path("tempFile"),filePath);
    }
    if(!quiet){
        std::cout << "\x1B[0;49;92m >\x1B[0m " << filePath.string().substr(strlen(appDir)) << " [" << archName << "]" << std::endl;
    }
}

int main(int argc, char *argv[])
{
    argv0 = argv[0];
    if(argc > 1){
        appDir = argv[1];
        fs::path fs_appDir(appDir);
        if(!fs::is_directory(fs_appDir)){
            print_error("Invalid Directory");
            print_usage();
        } else if(access(appDir, W_OK | X_OK) != 0){
            print_error("Directory not writeable. Copy it to another folder where it can be written to");
        } else {
            struct utsname unameBuff;
            uname(&unameBuff); 
            if(!strcmp(unameBuff.machine,arch)){
                remA64 = false;
            }

            for(int x = 2; x < argc; x++){
                const char* arg = argv[x];
                if(!strcmp(arg,"-q")){
                    quiet = true;
                } else if(!strcmp(arg,"-r")){
                    remA64 = !remA64;
                } else if(!strcmp(arg,"-p")){
                    printOnly = true;
                } else if(!strcmp(arg,"-b")){
                    if(access(fs_appDir.parent_path().string().c_str(), W_OK | X_OK) != 0){
                        print_error("Cannot make backup, parent directory not writeable");
                        exit(0);
                    } else {
                        std::cout << "\x1B[0;49;35m==>\x1B[0m \x1B[1mMaking Backup…\x1B[0m" << std::endl;
                        fs::path backupDir = fs_appDir;
                        backupDir.replace_filename(fs_appDir.stem().string() + "_Backup" + fs_appDir.extension().string());
                        if(fs::is_directory(backupDir)){
                            fs::remove_all(backupDir);
                        }
                        fs::copy(fs_appDir,backupDir,fs::copy_options::recursive | fs::copy_options::copy_symlinks);
                    }
                } else if(!strcmp(arg,"-c")){
                    if(access(fs_appDir.parent_path().string().c_str(), W_OK | X_OK) != 0){
                        print_error("Cannot make copy, parent directory not writeable");
                        exit(0);
                    } else {
                        std::cout << "\x1B[0;49;35m==>\x1B[0m \x1B[1mMaking and thinning copy…\x1B[0m" << std::endl;
                        fs::path fs_orig = fs_appDir;
                        fs_appDir.replace_filename(fs_appDir.stem().string() + "_ThinnedCopy" + fs_appDir.extension().string());
                        if(fs::is_directory(fs_appDir)){
                            fs::remove_all(fs_appDir);
                        }
                        fs::copy(fs_orig,fs_appDir,fs::copy_options::recursive | fs::copy_options::copy_symlinks);
                    }
                } else {
                    print_usage();
                }
            }

            if(!remA64){
                arch = "x86_64";
            }
            std::cout << "\x1B[0;49;34m==>\x1B[0m \x1B[1mRemoving " << arch << " portions…\x1B[0m" << std::endl;
            
            for(auto& p: fs::recursive_directory_iterator(fs_appDir)){
                if(fs::is_regular_file(p) && !fs::is_symlink(p)){
                    std::ifstream input(p.path(), std::ios::in|std::ios::binary);
                    input.seekg(0, std::ios::beg);
                    char* buffer = new char [4];
                    input.read(buffer,4);
                    if(buffer[0] == -54 && buffer[1] == -2 && buffer[2] == -70 && buffer[3] == -66){
                        input.read(buffer,4);
                        if(buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 0 && buffer[3] > 1){
                            char* cpu = new char[8];
                            bool has_x86 = false, has_arm64 = false, has_arm64e = false;
                            for(char i = 0; i < buffer[3]; i++){
                                input.read(cpu,8);
                                if(cpu[0] == 1 && cpu[3] == 7 && cpu[7] == 3 && cpu[1] == 0 && cpu[2] == 0 && cpu[4] == 0 && cpu[5] == 0 && cpu[6] == 0){
                                    has_x86 = true;
                                } else if(cpu[0] == 1 && cpu[3] == 12 && cpu[1] == 0 && cpu[2] == 0 && cpu[4] == 0 && cpu[5] == 0 && cpu[6] == 0 && cpu[7] == 0){
                                    has_arm64 = true;
                                } else if(cpu[0] == 1 && cpu[3] == 12 && cpu[4] == -128 && cpu[7] == 2 && cpu[1] == 0 && cpu[2] == 0 && cpu[5] == 0 && cpu[6] == 0){
                                    has_arm64e = true;
                                }
                                input.seekg(28 + i * 20);
                            }
                            if(has_x86 && has_arm64 && remA64){
                                thin_machO("arm64",p);
                            }
                            if(has_x86 && has_arm64e && remA64){
                                thin_machO("arm64e",p);
                            }
                            if((has_arm64 || has_arm64e) && has_x86 && !remA64){
                                thin_machO("x86_64",p);
                            }
                        }
                    }
                }
            }

            if(!hasUB){
                std::cout << "\x1B[0;49;94m==>\x1B[0m \x1B[1mNothing to thin…\x1B[0m" << std::endl;
            }

        }

    } else {
        print_usage();
    }
    return 0;
}
