#include "ccp/core.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
    ccp::Simulation simulation;
    std::string line;
    while(std::getline(std::cin,line)) {
        std::string requestId="null";
        const auto idAt=line.find("\"id\":");
        if(idAt!=std::string::npos){const auto begin=line.find_first_of("0123456789",idAt+5);if(begin!=std::string::npos){const auto end=line.find_first_not_of("0123456789",begin);requestId=line.substr(begin,end==std::string::npos?end:end-begin);}}
        try {
            if(line.find("\"command\":\"step\"")!=std::string::npos) simulation.step();
            else if(line.find("\"command\":\"reset\"")!=std::string::npos) {
                std::string scene="elastic";
                const auto at=line.find("\"scene\":\""); if(at!=std::string::npos){const auto start=at+9;const auto end=line.find('"',start);if(end!=std::string::npos)scene=line.substr(start,end-start);}
                simulation.reset(scene);
            } else if(line.find("\"command\":\"broadPhase\"")!=std::string::npos) {
                std::string mode="sweep";
                const auto at=line.find("\"mode\":\""); if(at!=std::string::npos){const auto start=at+8;const auto end=line.find('"',start);if(end!=std::string::npos)mode=line.substr(start,end-start);}
                simulation.setBroadPhase(mode);
            } else if(line.find("\"command\":\"queryMode\"")!=std::string::npos) {
                std::string mode="discrete";
                const auto at=line.find("\"mode\":\""); if(at!=std::string::npos){const auto start=at+8;const auto end=line.find('"',start);if(end!=std::string::npos)mode=line.substr(start,end-start);}
                simulation.setQueryMode(mode);
            } else if(line.find("\"command\":\"status\"")==std::string::npos) throw std::invalid_argument("unknown command");
            std::cout<<"{\"id\":"<<requestId<<",\"ok\":true,\"snapshot\":"<<ccp::toJson(simulation.snapshot())<<"}"<<std::endl;
        } catch(const std::exception& e) {
            std::cout<<"{\"id\":"<<requestId<<",\"ok\":false,\"error\":\""<<e.what()<<"\"}"<<std::endl;
        }
    }
}
