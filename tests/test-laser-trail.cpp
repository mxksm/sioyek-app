#include "../pdf_viewer/laser_trail.h"
#include <cassert>
#include <iostream>
int main() {
    for (double p : {0.,.25,.5,.75,1.}) {
        assert(laser_trail::opacity(p,-1)==1); // held, however long the gesture lasts
        assert(laser_trail::opacity(p,0)==1);
        assert(laser_trail::opacity(p,200)==1);
        assert(laser_trail::opacity(p,laser_trail::lifetime_ms)==0);
        double previous=1;
        for (int t=0;t<=2000;++t) {
            double alpha=laser_trail::opacity(p,t);
            assert(alpha>=0 && alpha<=previous); previous=alpha;
        }
    }
    assert(laser_trail::opacity(0,400)>0 && laser_trail::opacity(0,400)<1);
    assert(laser_trail::opacity(0,700)==0);
    assert(laser_trail::opacity(.5,700)==1);
    assert(laser_trail::opacity(1,1200)>0);
    std::cout << "PASS: held visibility, release delay, ordered smooth fade and complete expiry\n";
}
