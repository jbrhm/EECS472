#include <getopt.h>
#include <queue>
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
#include <string>
#include <cstring>
#include <cmath>

struct VcdKeyword{
    std::string pattern;
    std::function<void(std::ifstream&)> callback;
};

struct Timestamp{
    std::size_t time;
    std::string value;
    Timestamp(std::size_t t, std::string v) : time{t}, value{std::move(v)}{};
    ~Timestamp() = default;
};

class Variable{
private:
    std::vector<Timestamp> value;
    std::size_t num_nibbles;

public:
    static std::unordered_map<std::string, Variable> var_map; // map from vcd
    static std::unordered_map<std::string, std::string> rtl_to_vcd_names; // external names to internal names
    static std::unordered_map<std::string, std::string> vcd_to_rtl_names; // external names to internal names
    
    static std::string binary_to_hex(std::string binary, std::size_t num_hex){
        char const* hex_lut = "0123456789ABCDEF";
        assert(std::strlen(hex_lut) == 16);
        std::queue<std::size_t> x_indices{};
        for(std::size_t i = 0; i < binary.length(); ++i){
            if(binary[i] == 'x'){
                binary[i] = '0';
                std::size_t idx = binary.length() - i - 1;
                x_indices.push(idx/4);
            }
        }
        std::size_t decimal = std::stoull(binary, nullptr, 2);

        std::string output{};
        // loop over the 16 nibbles in the std::size_t
        for(std::size_t i = 0; i < num_hex; ++i){
            std::size_t idx = num_hex - i - 1;
            if(!x_indices.empty() && x_indices.front() == idx){
                output.push_back('x');
                x_indices.pop();
            }else{
                std::size_t mask = 0xFU << (idx * 4);
                output.push_back(hex_lut[(decimal & mask) >> (idx * 4)]);
            }
        }

        return output;
    }

    Variable() = default;
    Variable(std::size_t bits_in) : value{}, num_nibbles{static_cast<std::size_t>(std::ceil(bits_in / 4.0f))}{
        std::string init_value{};
        for(std::size_t i = 0; i < num_nibbles; ++i){
            init_value += "x";
        }
        value.emplace_back(0, init_value);
    }
    ~Variable() = default;

    void add_timestamp(std::size_t time_in, std::string value_in){
        value.emplace_back(time_in, value_in);
    }

    [[nodiscard]] Timestamp const& get_value(std::size_t time) const{
        std::size_t i = 0;
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

    [[nodiscard]] std::size_t get_size() const{
        return num_nibbles;
    }
};
std::unordered_map<std::string, Variable> Variable::var_map{};
std::unordered_map<std::string, std::string> Variable::rtl_to_vcd_names{};
std::unordered_map<std::string, std::string> Variable::vcd_to_rtl_names{};

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

                    Variable::var_map[vcd_name] = Variable(std::stoull(size));
                    Variable::rtl_to_vcd_names[rtl_name] = vcd_name;
                    Variable::vcd_to_rtl_names[vcd_name] = rtl_name;
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

    // create the tables with all of the variables
    std::string str{};
    while(vcd_file >> str){
        for(auto const& [pat, cb] : keywords){
            if(str == pat){
                cb(vcd_file);
            }
        }
    }

    // reset to the beginning
    vcd_file.clear();
    vcd_file.seekg(0, std::ios::beg);

    // parse all of the value changes
    std::string line{};
    std::size_t curr_timestamp = 0;
    while(std::getline(vcd_file, line)){
        std::stringstream line_stream(line);
        if(*line.begin() == '0' || *line.begin() == '1'){
            std::string val;
            if(line_stream >> val){
                assert(Variable::var_map[val.substr(1)].get_size() == 1);
                Variable::var_map[val.substr(1)].add_timestamp(curr_timestamp, (*line.begin() == '0') ? "0" : "1");
            }else{
                throw std::runtime_error("Expected one token in line...");
            }
        }else if(*line.begin() == 'b'){
            std::string value, name;
            if(line_stream >> value >> name){
                assert(Variable::var_map.contains(name));
                Variable::var_map[name].add_timestamp(curr_timestamp, Variable::binary_to_hex(value.substr(1), Variable::var_map[name].get_size()));
            }else{
                throw std::runtime_error("Expected two tokens in line...");
            }
        }else if(*line.begin() == '#'){
            curr_timestamp = std::stoull(line.substr(1));
        }
    }

    for(auto const& [n, v] : Variable::var_map){
        std::cout << std::format("Variable: {} Size: {}\n", Variable::vcd_to_rtl_names[n], Variable::var_map[n].get_size());
    }

    {
        std::string var{"stage_if_0.PC_reg"};
        std::size_t t = 0;
        std::cout << std::format("{} @ {}: {}\n", var, t, Variable::var_map[Variable::rtl_to_vcd_names[var]].get_value(t).value);
    }
    {
        std::string var{"stage_if_0.PC_reg"};
        std::size_t t = 150;
        std::cout << std::format("{} @ {}: {}\n", var, t, Variable::var_map[Variable::rtl_to_vcd_names[var]].get_value(t).value);
    }
    {
        std::string var{"stage_if_0.PC_reg"};
        std::size_t t = 300;
        std::cout << std::format("{} @ {}: {}\n", var, t, Variable::var_map[Variable::rtl_to_vcd_names[var]].get_value(t).value);
    }
    {
        std::string var{"stage_if_0.PC_reg"};
        std::size_t t = 450;
        std::cout << std::format("{} @ {}: {}\n", var, t, Variable::var_map[Variable::rtl_to_vcd_names[var]].get_value(t).value);
    }
}
