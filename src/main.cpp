#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>     // for getenv
#include <unistd.h>    // for access(), fork(), exec(), getcwd()
#include <sys/wait.h>  // for wait()
#include <limits.h>    // for PATH_MAX
#include <pwd.h>  // for getpwuid()
using namespace std;

bool is_builtin(const string& cmd) {
    return cmd == "echo" || cmd == "exit" || cmd == "type" || cmd == "pwd";
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

              if (!current_arg.empty()) {
                  args.push_back(current_arg);
                  current_arg.clear();
              }
              in_quote = false;
          } else {

              if (!current_arg.empty()) {
                  args.push_back(current_arg);
                  current_arg.clear();
              }
              in_quote = true;
          }
      } else if (isspace(c) && !in_quote) {

          if (!current_arg.empty()) {
              args.push_back(current_arg);
              current_arg.clear();
          }
      } else {

          current_arg += c;
      }
  }
  

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
  
  // Handle ~ at start of path
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

        if (cmd == "echo" && args.size() > 1) {
          // Find the position after "echo "
          size_t echo_pos = input.find("echo ");
          if (echo_pos != string::npos) {
              string echo_arg = input.substr(echo_pos + 5);
              cout << echo_arg << endl;
          }
        } else if (cmd == "type" && args.size() > 1) {
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
        } else if (cmd == "pwd") {
            handle_pwd();
        } else if(cmd == "cd") {
            handle_cd(args);
        } else {
            string cmd_path = find_in_path(cmd);
            if (!cmd_path.empty()) {
                char** argv = new char*[args.size() + 1];
                for (size_t i = 0; i < args.size(); i++) {
                    argv[i] = const_cast<char*>(args[i].c_str());
                }
                argv[args.size()] = nullptr;
                
                pid_t pid = fork();
                if (pid == 0) {
                    execv(cmd_path.c_str(), argv);
                    cerr << cmd << ": execution failed" << endl;
                    exit(1);
                } else if (pid > 0) {
                    wait(nullptr);
                } else {
                    cerr << "fork failed" << endl;
                }
                
                delete[] argv;
            } else {
                cout << cmd << ": command not found" << endl;
            }
        }
    }
    return 0;
}