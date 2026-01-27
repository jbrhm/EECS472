#include <getopt.h>
#include <queue>
#include <numeric>
#include <cassert>
#include <optional>
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
#include <charconv>

using extracted_string = std::optional<std::string_view>;

class FileExtractor{
private:
    std::string const& s;
    std::string::const_iterator beg;
    std::string::const_iterator end;

public:
    FileExtractor(std::string const& s_in) : s{s_in}, beg{s_in.begin()}, end{s_in.begin()}{}

    [[nodiscard]] extracted_string extract_line(){
        beg = end;
        while(beg != s.end() && std::isspace(*beg)){
            ++beg;
        }

        if(beg == s.end()){
            return std::nullopt;
        }

        end = beg;

        while(end != s.end() && !std::isspace(*end)){
            ++end;
        }

        if(beg == end){
            return std::nullopt;
        }

        return std::string_view(&(*beg), std::distance(beg, end));
    }
};

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
    static std::map<std::string, Variable, std::less<void>> var_map; // map from vcd
    static std::map<std::string, std::string, std::less<void>> rtl_to_vcd_names; // external names to internal names
    static std::map<std::string, std::string, std::less<void>> vcd_to_rtl_names; // external names to internal names
    
    static std::string binary_to_hex(std::string_view binary, std::size_t num_hex){
        char const* hex_lut = "0123456789ABCDEF";
        assert(std::strlen(hex_lut) == 16);

        std::string output{};
        output.reserve(num_hex);
        // handle the case where the size is not a multiple of four
        if(binary.length() % 4){
            std::size_t decimal;
            std::from_chars_result res = std::from_chars(
                binary.data(),
                binary.data() + (binary.length() % 4),
                decimal,
                2
            );

            if (res.ec == std::errc{}) {
                output.push_back(hex_lut[decimal]);
            } else if (res.ec == std::errc::invalid_argument) {
                output.push_back('x');
            } else if (res.ec == std::errc::result_out_of_range) {
                output.push_back('x');
            }
        }
        for(std::size_t i = binary.length() % 4; (i + 4) <= binary.length(); i += 4){
            std::size_t decimal;
            std::from_chars_result res = std::from_chars(
                binary.data() + i,
                binary.data() + i + 4,
                decimal,
                2
            );

            if (res.ec == std::errc{}) {
                output.push_back(hex_lut[decimal]);
            } else if (res.ec == std::errc::invalid_argument) {
                output.push_back('x');
            } else if (res.ec == std::errc::result_out_of_range) {
                output.push_back('x');
            }
        }

        return output;
    }

    static std::optional<std::size_t> get_clock_period(std::string const& clock_signal){
        if(Variable::rtl_to_vcd_names.contains(clock_signal) && Variable::var_map.contains(Variable::rtl_to_vcd_names[clock_signal])){
            Variable const& v = Variable::var_map[Variable::rtl_to_vcd_names[clock_signal]];
            std::size_t idx = v.get_num_timestamps() / 2;
            if(idx < v.get_num_timestamps() && (idx + 1) < v.get_num_timestamps()){
                assert(v.value[idx + 1].value != v.value[idx].value);
                return 2 * (v.value[idx + 1].time - v.value[idx].time);
            }else{
                std::cout << "Failed to detect clock period\n";
                return std::nullopt;
            }
        }else{
            throw std::runtime_error("Clock signal not defined...");
        }
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

    [[nodiscard]] std::size_t get_num_nibbles() const{
        return num_nibbles;
    }

    [[nodiscard]] std::size_t get_num_timestamps() const{
        return value.size();
    }
};
std::map<std::string, Variable, std::less<void>> Variable::var_map{};
std::map<std::string, std::string, std::less<void>> Variable::rtl_to_vcd_names{};
std::map<std::string, std::string, std::less<void>> Variable::vcd_to_rtl_names{};

char const* const shortopts = "v:hc:";

int main(int argc, char** argv){
    std::ios_base::sync_with_stdio(false);
    std::string file_name, clock_signal;
    while(true){
        int c;
        int idx;
        static struct option longopts[] = {
            {"vcd",     required_argument,  0, 'v'},
            {"help",    0,                  0, 'h'},
            {"clock",   required_argument,  0, 'c'},
            {0,         0,                  0, 0}
        };

        c = getopt_long(argc, argv, shortopts, longopts, &idx);

        if(c == -1)
            break;

        switch(c){
            case 'v':
                file_name = optarg;
                break;
            case 'c':
                clock_signal = optarg;
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

    std::ifstream vcd_file(file_name, std::ios::in | std::ios::binary);

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

    // for(auto const& [v, _] : Variable::rtl_to_vcd_names){
    //     std::cout << "Variable: " << v << '\n';
    // }

    // reset to the beginning
    vcd_file.clear();
    vcd_file.seekg(0, std::ios::beg);

    // parse all of the value changes
    std::string line{};
    std::size_t curr_timestamp = 0;
    while(std::getline(vcd_file, line)){
        FileExtractor fe(line);
        if(*line.begin() == '0' || *line.begin() == '1'){
            extracted_string val = fe.extract_line();
            if(val.has_value()){
                std::string_view::const_iterator new_beg = val.value().begin();
                new_beg = ++new_beg;
                std::string_view vcd_name(&(*new_beg), std::distance(new_beg, val.value().end()));
                auto it = Variable::var_map.find(vcd_name);

                assert(it != Variable::var_map.end());
                assert(it->second.get_num_nibbles() == 1);

                it->second.add_timestamp(curr_timestamp, (*line.begin() == '0') ? "0" : "1");
            }else{
                throw std::runtime_error("Expected one token in line...");
            }
        }else if(*line.begin() == 'b'){
        //     extracted_string value = fe.extract_line();
        //     extracted_string name = fe.extract_line();
        //     if(value.has_value() && name.has_value()){
        //         auto it = Variable::var_map.find(name.value());
        //         assert(it != Variable::var_map.end());
        //
        //         std::string_view::const_iterator new_beg = value.value().begin();
        //         new_beg = ++new_beg;
        //         value = std::string_view(&(*(new_beg)), std::distance(new_beg, value.value().end()));
        //
        //         it->second.add_timestamp(curr_timestamp, Variable::binary_to_hex(value.value(), it->second.get_num_nibbles()));
        //
        //     }else{
        //         throw std::runtime_error("Expected two tokens in line...");
        //     }
        }else if(*line.begin() == '#'){
            curr_timestamp = std::stoull(line.substr(1));
        }
    }

    // auto detect the clock period
    std::size_t clock_period = 100;
    std::optional<std::size_t> clock_period_opt = Variable::get_clock_period(clock_signal);
    if(clock_period_opt.has_value()){
        clock_period = clock_period_opt.value();
    }

    // COUNT THE TOTAL DATA HAZARDS //
    std::string var{"testbench.verisimpleV.data_hazard"};
    std::size_t total_data_hazards = 0;
    for(std::size_t i = 0; i < curr_timestamp; i += clock_period){
        std::string v = Variable::var_map[Variable::rtl_to_vcd_names[var]].get_value(i).value;
        if(v == "1"){
            ++total_data_hazards;
        }
    }

    std::cout << total_data_hazards <<'\n';
    // COUNT THE TOTAL DATA HAZARDS //

    // COUNT THE NUMBER OF FTBNT MISPREDICTED BRANCHES //
    std::string cond_branch_name{"testbench.verisimpleV.john_mem_wb_cond_branch_dbg"};
    std::string uncond_branch_name{"testbench.verisimpleV.john_mem_wb_uncond_branch_dbg"};
    std::string valid_name{"testbench.verisimpleV.john_mem_wb_valid_dbg"};
    std::string taken_name{"testbench.verisimpleV.john_mem_wb_take_branch_dbg"};
    std::string PC_name{"testbench.verisimpleV.john_mem_wb_PC_dbg"};
    std::string NPC_name{"testbench.verisimpleV.john_mem_wb_NPC_dbg"};

    std::size_t total_branches = 0;
    std::size_t correct_branches = 0;
    for(std::size_t i = 0; i < curr_timestamp; i += clock_period){
        std::string cond_branch = Variable::var_map[Variable::rtl_to_vcd_names[cond_branch_name]].get_value(i).value;
        std::string uncond_branch = Variable::var_map[Variable::rtl_to_vcd_names[uncond_branch_name]].get_value(i).value;
        std::string valid = Variable::var_map[Variable::rtl_to_vcd_names[valid_name]].get_value(i).value;
        std::string taken = Variable::var_map[Variable::rtl_to_vcd_names[taken_name]].get_value(i).value;
        std::string PC = Variable::var_map[Variable::rtl_to_vcd_names[PC_name]].get_value(i).value;
        std::string NPC = Variable::var_map[Variable::rtl_to_vcd_names[NPC_name]].get_value(i).value;

        if(valid == "1" && (cond_branch == "1" || uncond_branch == "1")){
            ++total_branches;
            std::size_t PC_val = std::stoull(PC, nullptr, 16);
            std::size_t NPC_val = std::stoull(NPC, nullptr, 16);
            if((NPC_val < PC_val && taken == "1") || (NPC_val > PC_val && taken == "0")){
                ++correct_branches;
            }
        }
    }

    std::cout << static_cast<double>(correct_branches) / total_branches <<'\n';
    // COUNT THE NUMBER OF FTBNT MISPREDICTED BRANCHES //
}
