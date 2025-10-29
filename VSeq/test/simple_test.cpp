#include <iostream>

struct Test {
    int gateCurrentStep;
    bool gateInSection2;
    int gateSection1Counter;
    int gateSection2Counter;
    bool gatePingpongForward;
    
    Test() {
        gateCurrentStep = 0;
        gateInSection2 = false;
        gateSection1Counter = 0;
        gateSection2Counter = 0;
        gatePingpongForward = true;
    }
    
    void advanceGate(int trackLength, int splitPoint, int sec1Reps, int sec2Reps) {
        int section1End = (splitPoint > 0 && splitPoint < trackLength) ? splitPoint : trackLength;
        
        std::cout << "Before: step=" << gateCurrentStep << ", inSec2=" << gateInSection2 << "\n";
        gateCurrentStep++;
        std::cout << "After increment: step=" << gateCurrentStep << "\n";
        std::cout << "section1End=" << section1End << ", splitPoint=" << splitPoint << "\n";
        
        if (!gateInSection2 && gateCurrentStep >= section1End) {
            std::cout << "Section 1 end triggered!\n";
            gateSection1Counter++;
            if (gateSection1Counter >= sec1Reps) {
                gateSection1Counter = 0;
                gateInSection2 = true;
                gateCurrentStep = (splitPoint > 0) ? splitPoint : 0;
                std::cout << "Moving to section 2, step=" << gateCurrentStep << "\n";
            } else {
                gateCurrentStep = 0;
                std::cout << "Repeating section 1, step=" << gateCurrentStep << "\n";
            }
        } else if (gateInSection2 && gateCurrentStep >= trackLength) {
            std::cout << "Section 2 end triggered!\n";
            gateSection2Counter++;
            if (gateSection2Counter >= sec2Reps) {
                gateSection2Counter = 0;
                gateInSection2 = false;
            }
            gateCurrentStep = (splitPoint > 0) ? splitPoint : 0;
            if (!gateInSection2) {
                gateCurrentStep = 0;
            }
        }
        std::cout << "Final: step=" << gateCurrentStep << ", inSec2=" << gateInSection2 << "\n\n";
    }
};

int main() {
    Test t;
    std::cout << "=== First Advance ===\n";
    t.advanceGate(16, 16, 1, 1);
    
    return 0;
}
