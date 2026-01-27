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
    std::regex pattern;
    std::function<void(std::string_view)> callback;
} VcdKeyword;

typedef struct{
    int time;
    std::string value;
} timestamp;

class Variable{
private:
    std::string name;
    std::vector<timestamp> value;

public:
    Variable() = default;
    Variable(std::string name_in, std::string value_in = "x") : name{std::move(name_in)}, value{}{
        value.push_back(timestamp{.time{}, .value{value_in}});
    }
    ~Variable() = default;

    void add_timestamp(int time, std::string value_in){
        value.push_back(timestamp{.time=time, .value{value_in}});
    }

    [[nodiscard]] std::string const& get_name() const{
        return name;
    }

    [[nodiscard]] std::vector<timestamp> const& get_value() const{
        return value;
    }

    [[nodiscard]] timestamp get_value(int time) const{
        int i = 0;
        bool found = false;
        for(; i < value.size(); ++i){
            timestamp const& v = value[i];

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

    std::vector<std::string> scopes;

    std::vector<VcdKeyword> const keywords {
        VcdKeyword{
            .pattern=std::regex{"\\$timescale", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                // std::cout << std::format("Found {}", s);
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$date", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                // std::cout << std::format("Found {}", s);
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$comment", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                // std::cout << std::format("Found {}", s);
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$version", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                // std::cout << std::format("Found {}", s);
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$scope", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                std::string in(s);
                std::string module, _;
                if(std::stringstream{in} >> _ >> module){
                    scopes.push_back(module);
                }else{
                    throw std::runtime_error("Error expected two arguments in scope statement");
                }
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$upscope", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                scopes.pop_back();
            }}},
        VcdKeyword{
            .pattern=std::regex{"\\$var", std::regex_constants::extended}, 
            .callback{[&](std::string_view s){
                std::string in(s);
                std::string type, size, vcd_name, var_name;
                if(std::stringstream{in} >> type >> size >> vcd_name >> var_name){
                    // std::cout << std::format("type: {} size: {} vcd_name: {} var_name {}\n", type, size, vcd_name, var_name);
                    for(std::string const& s : scopes){
                        var_name = s + "." + var_name;
                    }
                    var_map[vcd_name] = Variable(var_name);
                    rtl_to_vcd_names[var_name] = vcd_name;
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
            // std::cout << std::format("Didn't find start in {}\n", buffer);
            increment_file_pos(buffer.size());
            continue;
        }

        std::string_view data;

        // if we did find the start of a keyword, search for the end
        bool found_end = std::regex_search(buffer, end, END_REGEX);
        if(!found_end && buffer.size() == BUFFER_SIZE){
            throw std::runtime_error("BUFFER_SIZE is not large enough");
        }else if(!found_end){
            // std::cout << std::format("Didn't find end in {}\n", buffer);
            increment_file_pos(buffer.size());
            continue;
        }

        // at this point we know that the operation has succeded
        assert(found_end);

        // use string view to avoid copying the data
        // thank goodness for c++20
        data = std::string_view(buffer.begin() + start.length(), buffer.begin() + end.position());

        cb(data);

        // this line should not be able to throw an exception since exceptions are thrown for pos > size not pos == size
        buffer = buffer.substr(end.position() + end.length()); 
        increment_file_pos(end.position() + end.length());
    }

    for(auto const& [key, val] : var_map){
        // std::cout << std::format("key: {} val: {}\n", key, val.get_name());
    }

    // reset back to the beginning
    vcd_file.clear();
    vcd_file.seekg(0, std::ios::beg);

    std::string pass_line;
    int current_timestamp = 0;
    while(std::getline(vcd_file, pass_line)){
        std::stringstream ss{pass_line};

        std::string value, name;
        if(*pass_line.begin() == 'b'){
            if(!(ss >> value >> name)){
                throw std::runtime_error("Error binary value not found");
            }

            value = value.substr(1);
        }else if(*pass_line.begin() == '#'){
            std::string str_timestamp;
            if(!(ss >> str_timestamp)){
                throw std::runtime_error("Error timestamp not found");
            }

            current_timestamp = std::stoi(str_timestamp.substr(1));
        }else if(*pass_line.begin() == '0' || *pass_line.begin() == '1'){
            value = std::format("{}", *pass_line.begin());
            name = pass_line.substr(1);
        }

        var_map[name].add_timestamp(current_timestamp, value);
    }

    std::vector<int> counters{};
    int curr_count = 0;

    for(int i = 0; i * 150 <= 106318220; ++i){
        // Variable const& v = var_map[rtl_to_vcd_names["testbench.$unit.clock"]];
        Variable const& valid = var_map[rtl_to_vcd_names["verisimpleV.unnamed$$_0.output_reg_writeback_and_maybe_halt.unnamed$$_0.show_final_mem_and_status.output_cpi_file.testbench.$unit.mem_wb_valid_dbg"]];
        Variable const& take = var_map[rtl_to_vcd_names["verisimpleV.unnamed$$_0.output_reg_writeback_and_maybe_halt.unnamed$$_0.show_final_mem_and_status.output_cpi_file.testbench.$unit.take_branch"]];
        // std::cout << std::format("Timestamp: {} Value: {}\n", 150 * i, v.get_value(150 * i).value);
        std::cout << std::format("Timestamp: {} Value: {}\n", 150 * i, valid.get_value(150 * i).value);
        std::cout << std::format("Timestamp: {} Value: {}\n", 150 * i, take.get_value(150 * i).value);

        if(valid.get_value(150 * i).value == "1"){
            ++curr_count;
        }
        if(take.get_value(150 * i).value == "1"){
            counters.push_back(curr_count);
            curr_count = 0;
        }
    }
    counters.push_back(curr_count);

    std::cout << counters.size() << '\n';
    std::cout << std::format("Average Basic Block Size {}\n", std::accumulate(std::begin(counters), std::end(counters), 0) / static_cast<float>(counters.size())) << '\n';
}
