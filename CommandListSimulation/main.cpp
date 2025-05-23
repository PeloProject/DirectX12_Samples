#include <vector>
#include <functional>
#include <iostream>

using namespace std;


int main()
{
	vector<function<void()>> commandList;

	commandList.push_back([]() { cout << "GPU Set RTV - �@" << endl; }); // ���߂P
	cout << "GPU Set RTV - �A" << endl;

	commandList.push_back([]() { cout << "GPU Clear RTV - �B" << endl; }); // ���߂Q
	cout << "GPU Clear RTV - �C" << endl; 

	commandList.push_back([]() { cout << "GPU Close RTV - �D" << endl; }); // ���߂R
	cout << "GPU Close RTV - �E" << endl;

	cout << endl;

	// �R�}���h���X�g�̎��s
	for (auto& command : commandList)
	{
		command(); // �R�}���h�����s
	}

	getchar(); // �R���\�[����ʂ���Ȃ��悤�ɂ��邽�߂̓��͑҂�

	return 0;
}