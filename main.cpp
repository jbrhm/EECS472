#include <getopt.h>
#include <numeric>
#include <cassert>
#include <format>
#include <iostream>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <cstdint>
#include <sstream>
#include <functional>

// Header
typedef struct {
    std::string pattern;
    std::function<void(std::ifstream&)> callback;
} VcdKeyword;

typedef struct{
    int time;
    std::string value;
} Timestamp;

class Variable{
private:
    std::string name;
    std::vector<Timestamp> value;

public:
    Variable() = default;
    Variable(std::string name_in, std::string value_in = "x") : name{std::move(name_in)}, value{}{
        value.push_back(Timestamp{.time{}, .value{value_in}});
    }
    ~Variable() = default;

    void add_timestamp(int time, std::string value_in){
        value.push_back(Timestamp{.time=time, .value{value_in}});
    }

    [[nodiscard]] std::string const& get_name() const{
        return name;
    }

    [[nodiscard]] std::vector<Timestamp> const& get_value() const{
        return value;
    }

    [[nodiscard]] Timestamp get_value(int time) const{
        int i = 0;
        bool found = false;
        for(; i < value.size(); ++i){
            Timestamp const& v = value[i];

            if(i + 1 < value.size() && value[i + 1].time > time){
                found = true;
                break;
            }
        }

        if(!found){
            return value.back();
        }

        return value[i];
    }
};

std::unordered_map<std::string, Variable> var_map; // map from vcd
std::unordered_map<std::string, std::string> rtl_to_vcd_names; // external names to internal names

char const* const shortopts = "v:h";

int main(int argc, char** argv){
    std::ios_base::sync_with_stdio(false);
    std::string_view file_name;
    while(true){
        int c;
        int idx;
        static struct option longopts[] = {
            {"vcd",     required_argument,  0, 'v'},
            {"help",    0,                  0, 'h'},
            {0,         0,                  0, 0}
        };

        c = getopt_long(argc, argv, shortopts, longopts, &idx);

        if(c == -1)
            break;

        switch(c){
            case 'v':
                file_name = optarg;
                break;
            case 'h':
                std::cout << "Usage: ./vcd-analysis [--help | --vcd=path/to/vcd/file]";
                exit(0);
                break;
            default:
                std::cerr << "Unknown command line argument\n";
                exit(1);
                break;
        }
    }

    if(file_name.empty()){
        std::cerr << "No VCD file provided\n";
        return 1;
    }

    std::ifstream vcd_file(static_cast<std::string>(file_name), std::ios::in | std::ios::binary);

    if(!vcd_file.is_open()){
        std::cerr << "No such file\n";
        return 1;
    }

    static constexpr char const* END_KEYWORD = "$end";

    std::vector<std::string> module_stack{};
    std::vector<VcdKeyword> const keywords {
        VcdKeyword{
            .pattern="$timescale",
            .callback{[&](std::ifstream& fs){
                std::string str{};
                while(fs >> str && str != END_KEYWORD){
                    // std::cout << str << '\n';
                }
            }}},
        VcdKeyword{
            .pattern="$date",
            .callback{[&](std::ifstream& fs){
                std::string str{};
                while(fs >> str && str != END_KEYWORD){
                    // std::cout << str << '\n';
                }
            }}},
        VcdKeyword{
            .pattern="$comment",
            .callback{[&](std::ifstream& fs){
                std::string str{};
                while(fs >> str && str != END_KEYWORD){
                    // std::cout << str << '\n';
                }
            }}},
        VcdKeyword{
            .pattern="$version",
            .callback{[&](std::ifstream& fs){
                std::string str{};
                while(fs >> str && str != END_KEYWORD){
                    // std::cout << str << '\n';
                }
            }}},
        VcdKeyword{
            .pattern="$scope",
            .callback{[&](std::ifstream& fs){
                std::string _, name, end;
                if(fs >> _ >> name){
                    module_stack.push_back(name);
                }else{
                    throw std::runtime_error("Expected two tokens in scope statement...");
                }

            }}},
        VcdKeyword{
            .pattern="$upscope", 
            .callback{[&](std::ifstream& fs){
                std::string end;
                assert(!module_stack.empty());

                module_stack.pop_back();
                assert(fs >> end && end == END_KEYWORD);
            }}},
        VcdKeyword{
            .pattern="$var", 
            .callback{[&](std::ifstream& fs){
                std::string type, size, vcd_name, rtl_name, end;
                if(fs >> type >> size >> vcd_name >> rtl_name){
                    std::string module{};
                    for(std::string const& s : module_stack){
                        module += s + ".";
                    }

                    rtl_name = module + rtl_name;

                    std::cout << std::format("Type: {} Size: {} VCD Name: {} RTL Name: {}\n", type, size, vcd_name, rtl_name);
                }else{
                    throw std::runtime_error("Expected four tokens in scope statement...");
                }

                // this statement exists to support the [31:0] syntax
                if(fs >> end && end == END_KEYWORD){
                    return;
                }
                assert(fs >> end && end == END_KEYWORD);
            }}}
    };

    std::string str{};
    while(vcd_file >> str){
        for(auto const& [pat, cb] : keywords){
            if(str == pat){
                cb(vcd_file);
            }
        }
    }
}
