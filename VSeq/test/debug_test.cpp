#include <iostream>

int main() {
    int gateCurrentStep = 0;
    bool gateInSection2 = false;
    int gateSection1Counter = 0;
    int gateSection2Counter = 0;
    
    int trackLength = 16;
    int splitPoint = 16;
    int sec1Reps = 1;
    int sec2Reps = 1;
    
    int section1End = (splitPoint > 0 && splitPoint < trackLength) ? splitPoint : trackLength;
    
    std::cout << "Initial: step=" << gateCurrentStep << ", sec1End=" << section1End << "\n";
    
    // First advance
    gateCurrentStep++;
    std::cout << "After increment: step=" << gateCurrentStep << "\n";
    
    if (!gateInSection2 && gateCurrentStep >= section1End) {
        std::cout << "Triggering section1 end logic\n";
        gateSection1Counter++;
        if (gateSection1Counter >= sec1Reps) {
            gateSection1Counter = 0;
            gateInSection2 = true;
            gateCurrentStep = (splitPoint > 0) ? splitPoint : 0;
        } else {
            gateCurrentStep = 0;
        }
    } else if (gateInSection2 && gateCurrentStep >= trackLength) {
        std::cout << "Triggering section2 end logic\n";
    }
    
    std::cout << "Final: step=" << gateCurrentStep << ", inSec2=" << gateInSection2 << "\n";
    
    return 0;
}
