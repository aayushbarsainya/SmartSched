#include<iostream>
using namespace std;

int main(){
    long long A,B,C,D;
    cin>>A>>B>>C>>D;

    long long mul=((((A%100)*B%100)*C%100)*D%100)%100;

    if(mul<10) cout<<"0";
    cout<<mul;
}