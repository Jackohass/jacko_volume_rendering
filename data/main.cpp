#include<iostream>
#include<cstdlib>
#include<string>
#include<fstream>

using namespace std;

string names[] = {"AB", "AI", "ARB", "ARE", "ARV", "BB", "BI", "BRB", "BRE",
    "BRV", "EB", "EI", "ERB", "ERE", "ERV", "VB", "VI", "VRB", "VRE", "VRV"};

string nums[] = {"0", "1", "2"};

int main(int argc, char *argv[]) {
    
    ofstream file("autoEval.csv");
    if (!file.is_open()) {                        
        cout<<"File doesn’t exist.";
        exit(0);   
    }
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            string tar = nums[i] + "/" + names[j] + "/" + "eval.csv";
            ifstream anEval(tar);
            string line;
            if (!anEval.is_open()) {                        
                cout<<"anEval doesn’t exist." << tar;
                exit(0);   
            }
            while(getline(anEval, line))
            {
                file << line;
            }
            file << nums[i] << names[j] << "\n";
        }
    }
    file.close();

    return 0;
}