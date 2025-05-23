#include <vector>
#include <functional>
#include <iostream>

using namespace std;


int main()
{
	vector<function<void()>> commandList;

	commandList.push_back([]() { cout << "GPU Set RTV - ‡@" << endl; }); // –½—ß‚P
	cout << "GPU Set RTV - ‡A" << endl;

	commandList.push_back([]() { cout << "GPU Clear RTV - ‡B" << endl; }); // –½—ß‚Q
	cout << "GPU Clear RTV - ‡C" << endl; 

	commandList.push_back([]() { cout << "GPU Close RTV - ‡D" << endl; }); // –½—ß‚R
	cout << "GPU Close RTV - ‡E" << endl;

	cout << endl;

	// ƒRƒ}ƒ“ƒhƒŠƒXƒg‚ÌŽÀs
	for (auto& command : commandList)
	{
		command(); // ƒRƒ}ƒ“ƒh‚ðŽÀs
	}

	getchar(); // ƒRƒ“ƒ\[ƒ‹‰æ–Ê‚ð•Â‚¶‚È‚¢‚æ‚¤‚É‚·‚é‚½‚ß‚Ì“ü—Í‘Ò‚¿

	return 0;
}