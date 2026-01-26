#include <getopt.h>
#include <format>
#include <iostream>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <cstdint>
#include <sstream>

std::regex var_regex("\\$var (wire|reg) [1-9][0-9]* .+ .+", std::regex_constants::extended);

class Variable{
private:
    std::string name;
    std::vector<uint8_t> value;

public:
    Variable() = default;
    Variable(std::string name_in, std::vector<uint8_t> value_in = std::vector<uint8_t>{}) : name{std::move(name_in)}, value{std::move(value_in)}{}
    ~Variable() = default;

    [[nodiscard]] std::string const& get_name() const{
        return name;
    }

    [[nodiscard]] std::vector<uint8_t> const& get_value() const{
        return value;
    }
};

std::unordered_map<std::string, Variable> var_map;

char const* const shortopts = "v:h";

int main(int argc, char** argv){
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
                std::cout << std::format("VCD file {}\n", optarg);
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

    std::ifstream vcd_file(static_cast<std::string>(file_name));
    std::string line;

    while(std::getline(vcd_file, line)){
        if(std::regex_match(line, var_regex)){
            std::string _, type, size, vcd_name, var_name;
            if(std::stringstream{line} >> _ >> type >> size >> vcd_name >> var_name){
                std::cout << std::format("type: {} size: {} vcd_name: {} var_name {}\n", type, size, vcd_name, var_name);
                var_map[vcd_name] = Variable(var_name);
            }else{
                throw std::runtime_error("Expected to read 5 arguments...");
            }
        }
    }

    for(auto const& [key, val] : var_map){
        std::cout << std::format("key: {} val: {}\n", key, val.get_name());
    }
}
