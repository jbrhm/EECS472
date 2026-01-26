#include <getopt.h>
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
    std::regex pattern;
    std::function<void(std::string_view)> callback;
} VcdKeyword;


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

    std::ifstream vcd_file(static_cast<std::string>(file_name), std::ios::in | std::ios::binary);

    if(!vcd_file.is_open()){
        std::cerr << "No such file\n";
        return 1;
    }

    std::string line;

    std::vector<VcdKeyword> const keywords {
        VcdKeyword{
            .pattern=std::regex{"\\$timescale", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                std::cout << std::format("Found {}", s);
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$date", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                std::cout << std::format("Found {}", s);
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$comment", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                std::cout << std::format("Found {}", s);
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$version", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                std::cout << std::format("Found {}", s);
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$var", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                std::string in(s);
                std::string type, size, vcd_name, var_name;
                if(std::stringstream{in} >> type >> size >> vcd_name >> var_name){
                    std::cout << std::format("type: {} size: {} vcd_name: {} var_name {}\n", type, size, vcd_name, var_name);
                    var_map[vcd_name] = Variable(var_name);
                }else{
                    throw std::runtime_error(std::format("Expected to read 4 arguments. Instead got {}", in));
                }
            }}}
    };

    std::size_t file_pos = 0;
    std::function<void(std::size_t)> increment_file_pos = [&](std::size_t delta){
        file_pos += delta;
        std::cout << std::format("Seeking {}\n", file_pos);
        vcd_file.seekg(file_pos);
    };

    static constexpr std::size_t BUFFER_SIZE = 500;
    std::string buffer{};
    // TODO(JOHN): this doesnt handle end of file conditions perfectly
    while((buffer.resize(BUFFER_SIZE), vcd_file.read(buffer.data(), BUFFER_SIZE), vcd_file.gcount() > 0)){
        buffer.resize(vcd_file.gcount());

        std::regex const END_REGEX = std::regex{"\\$end"};

        std::smatch start, end;
        bool found_start = false;
        std::function<void(std::string_view)> cb;
        // search for the beginning keyword
        for(auto const& [pattern, callback] : keywords){
            // this is the dumbest thing ever, std::regex_search requires a full
            // copy of the string, it cannot be passed a string_view >:(
            if(std::regex_search(buffer, start, pattern)){
                increment_file_pos(start.position());

                // this line is bloat because it will do a large number of unecessary copying
                // >:(
                buffer = buffer.substr(start.position());
                found_start = true;
                cb = callback;
                break;
            }
        }

        // if we didn't find the start of a keyword, move to next portion of the buffer
        if(!found_start){
            std::cout << std::format("Didn't find start in {}\n", buffer);
            increment_file_pos(buffer.size());
            continue;
        }

        std::string_view data;

        // if we did find the start of a keyword, search for the end
        bool found_end = std::regex_search(buffer, end, END_REGEX);
        if(!found_end && buffer.size() == BUFFER_SIZE){
            throw std::runtime_error("BUFFER_SIZE is not large enough");
        }else if(!found_end){
            std::cout << std::format("Didn't find end in {}\n", buffer);
            increment_file_pos(buffer.size());
            continue;
        }

        // at this point we know that the operation has succeded
        assert(found_end);

        // use string view to avoid copying the data
        // thank goodness for c++20
        data = std::string_view(buffer.begin() + start.length(), buffer.begin() + end.position());

        std::cout << "//////////////////////" << '\n';
        cb(data);
        std::cout << "//////////////////////" << '\n';

        // this line should not be able to throw an exception since exceptions are thrown for pos > size not pos == size
        buffer = buffer.substr(end.position() + end.length()); 
        increment_file_pos(end.position() + end.length());
    }

    for(auto const& [key, val] : var_map){
        std::cout << std::format("key: {} val: {}\n", key, val.get_name());
    }
}
