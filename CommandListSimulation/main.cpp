#include <vector>
#include <functional>
#include <iostream>

using namespace std;


int main()
{
	vector<function<void()>> commandList;

	commandList.push_back([]() { cout << "GPU Set RTV - �@" << endl; }); // 命令１
	cout << "GPU Set RTV - �A" << endl;

	commandList.push_back([]() { cout << "GPU Clear RTV - �B" << endl; }); // 命令２
	cout << "GPU Clear RTV - �C" << endl; 

	commandList.push_back([]() { cout << "GPU Close RTV - �D" << endl; }); // 命令３
	cout << "GPU Close RTV - �E" << endl;

	cout << endl;

	// コマンドリストの実行
	for (auto& command : commandList)
	{
		command(); // コマンドを実行
	}

	getchar(); // コンソール画面を閉じないようにするための入力待ち

	return 0;
}