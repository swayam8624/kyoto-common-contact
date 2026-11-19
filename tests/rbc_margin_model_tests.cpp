// RBC closed-form stylized margin proxy: math regression, NOT a native
// performance benchmark or empirical optimal-margin validation.
#include "ccp/rbc/margin_cost_model.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
void check(bool cond,const char* reason) {
    if(!cond)throw std::runtime_error(reason);
}
template<class Fn>
void bad(Fn f) {
    try {f();} catch(const std::invalid_argument&) {return;}
    throw std::runtime_error("RBC invalid theoretical model input not rejected");
}
void test_known_model() {
    using namespace ccp::rbc;
    MarginModel m{20.,1.,.5,1.,.01};
    double optimum=predicted_unconstrained_margin(m);
    check(optimum>.187 && optimum<.189,
          "analytic unconstrained optimum disagrees with root");
    check(std::abs(predicted_derivative(m,optimum))<1e-10,
          "analytic optimum derivative not zero");
    const auto center=predicted_cost(m,optimum);
    check(center<3.0,"expected model cost incorrect");
    check(predicted_cost(m,optimum*.5)>center,
          "smaller margin beat exact convex proxy optimum");
    check(predicted_cost(m,optimum*2)>center,
          "larger margin beat exact convex proxy optimum");
    m.mean_step_drift=0;
    check(predicted_unconstrained_margin(m)==0,
          "static stream must not have positive optimal skin in proxy");
    check(std::abs(predicted_cost(m,0)-.5)<1e-14,
          "zero drift/zero skin proxy cost incorrect");
    m.mean_step_drift=1;
    bad([&]{(void)predicted_cost(m,0);});
    m.mean_step_drift=-1;
    bad([&]{(void)predicted_unconstrained_margin(m);});
    m.mean_step_drift=1;
    m.density=0;
    bad([&]{(void)predicted_unconstrained_margin(m);});
}
void test_random_stability_and_convexity() {
    using namespace ccp::rbc;
    std::mt19937 gen(82193);
    std::uniform_real_distribution<double> exp(-3.0,3.0);
    auto log_sample=[&]{return std::pow(10.,exp(gen));};
    for(int case_id=0;case_id<4000;++case_id) {
        MarginModel m{log_sample(),log_sample(),log_sample(),
                      log_sample(),log_sample()};
        const double optimum=predicted_unconstrained_margin(m);
        check(std::isfinite(optimum) && optimum>0,
              "analytic proxy returned nonfinite optimum");
        const double d=predicted_derivative(m,optimum);
        const double dscale=6.0*m.filter_cost*m.density*
                            std::pow(m.primitive_extent+2*optimum,2);
        check(std::abs(d)<=1e-9*dscale,
              "unstable closed-form derivative cancellation");
        const double base=predicted_cost(m,optimum);
        for(double factor:{.25,.5,.9,1.1,2.,4.}) {
            const double cost=predicted_cost(m,optimum*factor);
            check(cost>=base*(1.-1e-12),
                  "claimed strict convexity does not hold locally");
        }
    }
}
} // namespace

int main() {
    try{
        test_known_model();
        test_random_stability_and_convexity();
        std::cout<<"PASS RBC theoretical-margin proxy: "
                 <<"closed-form 4000-model derivative and minimum tests; "
                 <<"does not imply empirical speed or novelty\n";
    } catch(const std::exception& e){
        std::cerr<<"FAIL RBC margin-model proxy: "<<e.what()<<"\n";
        return 1;
    }
}
