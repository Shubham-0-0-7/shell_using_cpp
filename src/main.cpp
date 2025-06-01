#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>     // for getenv
#include <unistd.h>    // for access(), fork(), exec(), getcwd(), chdir()
#include <sys/wait.h>  // for wait()
#include <limits.h>    // for PATH_MAX
#include <pwd.h>       // for getpwuid()
#include <cstring>     // for strdup
#include <cerrno>      // for errno
using namespace std;

bool is_builtin(const string& cmd) {
    return cmd == "echo" || cmd == "exit" || cmd == "type" || cmd == "pwd" || cmd == "cd";
}

string find_in_path(const string& cmd) {
    const char* path_env = getenv("PATH");
    if (!path_env) return "";

    stringstream ss(path_env);
    string dir;

    while (getline(ss, dir, ':')) {
        string full_path = dir + "/" + cmd;
        if (access(full_path.c_str(), X_OK) == 0) {
            return full_path;
        }
    }
    return "";
}

vector<string> split_args(const string& input) {
    vector<string> args;
    bool in_quote = false;
    string current_arg;
    
    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];
        
        if (c == '\'') {
            if (in_quote) {
                // Closing quote - add the accumulated argument
                if (!current_arg.empty()) {
                    args.push_back(current_arg);
                    current_arg.clear();
                }
                in_quote = false;
            } else {
                // Opening quote - save any previous argument
                if (!current_arg.empty()) {
                    args.push_back(current_arg);
                    current_arg.clear();
                }
                in_quote = true;
            }
        } else if (isspace(c) && !in_quote) {
            // Space outside quotes - finish current argument
            if (!current_arg.empty()) {
                args.push_back(current_arg);
                current_arg.clear();
            }
        } else {
            // Regular character - add to current argument
            current_arg += c;
        }
    }
    
    // Add the last argument if exists
    if (!current_arg.empty()) {
        args.push_back(current_arg);
    }
    
    return args;
}

void handle_pwd() {
    char buffer[PATH_MAX];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        cout << buffer << endl;
    } else {
        cerr << "pwd: error getting current directory" << endl;
    }
}

string expand_tilde(const string& path) {
    if (path.empty()) return path;
    
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (home == nullptr) {
            home = getpwuid(getuid())->pw_dir;
        }
        if (home != nullptr) {
            if (path.size() == 1) {  // just "~"
                return home;
            } else if (path[1] == '/') {  // "~/something"
                return string(home) + path.substr(1);
            }
        }
    }
    return path;
}

void handle_cd(const vector<string>& args) {
    if (args.size() < 2) {
        const char* home = getenv("HOME");
        if (home == nullptr) {
            cerr << "cd: HOME not set" << endl;
            return;
        }
        if (chdir(home) != 0) {
            perror("cd");
        }
        return;
    }

    string path = expand_tilde(args[1]);
    if (chdir(path.c_str()) != 0) {
        perror(("cd: " + path).c_str());
    }
}

string strip_quotes(const string& input) {
    if (input.size() >= 2 && input.front() == '\'' && input.back() == '\'') {
        return input.substr(1, input.size() - 2);
    }
    return input;
}

int main() {
    cout << std::unitbuf;
    cerr << std::unitbuf;

    string input;
    while (true) {
        cout << "$ ";
        if (!getline(cin, input)) break;

        if (input.empty()) continue;

        if (input == "exit 0") break;

        vector<string> args = split_args(input);
        if (args.empty()) continue;

        string cmd = args[0];

        if (cmd == "echo") {
            if (args.size() > 1) {
                // Get everything after "echo " while preserving spaces
                size_t echo_pos = input.find("echo ");
                if (echo_pos != string::npos) {
                    string echo_arg = input.substr(echo_pos + 5);
                    // Strip only the outer quotes if they exist
                    echo_arg = strip_quotes(echo_arg);
                    cout << echo_arg << endl;
                }
            } else {
                cout << endl;  // echo without arguments prints newline
            }
        } else if (cmd == "type") {
            if (args.size() > 1) {
                string target_cmd = args[1];
                if (is_builtin(target_cmd)) {
                    cout << target_cmd << " is a shell builtin" << endl;
                } else {
                    string path = find_in_path(target_cmd);
                    if (!path.empty()) {
                        cout << target_cmd << " is " << path << endl;
                    } else {
                        cout << target_cmd << ": not found" << endl;
                    }
                }
            } else {
                cerr << "type: missing argument" << endl;
            }
        } else if (cmd == "pwd") {
            handle_pwd();
        } else if (cmd == "cd") {
            handle_cd(args);
        } else {
            string cmd_path = find_in_path(cmd);
            if (!cmd_path.empty()) {
                // Prepare arguments with quote stripping
                char** argv = new char*[args.size() + 1];
                for (size_t i = 0; i < args.size(); i++) {
                    string arg = strip_quotes(args[i]);
                    argv[i] = strdup(arg.c_str());
                }
                argv[args.size()] = nullptr;
                
                pid_t pid = fork();
                if (pid == 0) {
                    // Child process
                    execv(cmd_path.c_str(), argv);
                    // If execv returns, there was an error
                    cerr << cmd << ": " << strerror(errno) << endl;
                    exit(1);
                } else if (pid > 0) {
                    // Parent process
                    wait(nullptr);
                } else {
                    cerr << "fork failed" << endl;
                }
                
                // Clean up allocated memory
                for (size_t i = 0; i < args.size(); i++) {
                    free(argv[i]);
                }
                delete[] argv;
            } else {
                cout << cmd << ": command not found" << endl;
            }
        }
    }

    return 0;
}