#pragma once
#include	<dinput.h>

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")	

class CDirectInput{
private:
	LPDIRECTINPUT8			m_dinput{};				// DirectInput8�I�u�W�F�N�g
	LPDIRECTINPUTDEVICE8	m_dikeyboard{};			// �L�[�{�[�h�f�o�C�X
	LPDIRECTINPUTDEVICE8	m_dimouse{};			// �}�E�X�f�o�C�X
	char					m_keybuffer[256]{};		// �L�[�{�[�h�o�b�t�@
	char					m_oldkeybuffer[256]{};	// �O��̓��̓L�[�{�[�h�o�b�t�@
	DIMOUSESTATE2			m_MouseState{};			// �}�E�X�̏��
	DIMOUSESTATE2			m_MouseStateTrigger{};	// �}�E�X�̏��
	POINT					m_MousePoint{};			// �}�E�X���W
	int						m_width{};				// �}�E�X�̂w���W�ő�
	int						m_height{};				// �}�E�X�̂x���W�ő�
	HWND					m_hwnd{};
	CDirectInput() :m_dinput(nullptr), m_dikeyboard(nullptr), m_dimouse(nullptr) {
	}
public:

	CDirectInput(const CDirectInput&) = delete;
	CDirectInput& operator=(const CDirectInput&) = delete;
	CDirectInput(CDirectInput&&) = delete;
	CDirectInput& operator=(CDirectInput&&) = delete;

	static CDirectInput& GetInstance(){
		static CDirectInput Instance;
		return Instance;
	}

	~CDirectInput(){
		Dispose();
	}

	//----------------------------------
	// DirectInput ��������
	//
	//		P1 : �C���X�^���X�l
	//		P2 : �E�C���h�E�n���h���l
	//
	//	�߂�l
	//		true : ����������
	//		false : ���������s
	//----------------------------------
	bool Init(HINSTANCE hInst,HWND hwnd,int width,int height){
		HRESULT	hr;
		hr = DirectInput8Create(hInst, DIRECTINPUT_VERSION, IID_IDirectInput8, (void **)&m_dinput, NULL);
		if(FAILED(hr)) {
			return false;
		}

		// �L�[�{�[�h�f�o�C�X����
		m_dinput->CreateDevice(GUID_SysKeyboard, &m_dikeyboard, NULL);
		if(FAILED(hr)) {
			return false;
		}

		// �f�[�^�t�H�[�}�b�g�̐ݒ�
		hr = m_dikeyboard->SetDataFormat(&c_dfDIKeyboard);
		if(FAILED(hr)) {
			return false;
		}
		
		// �������x���̐ݒ�
		hr = m_dikeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		if(FAILED(hr)) {
			return false;
		}

		// �}�E�X�f�o�C�X����
		m_dinput->CreateDevice(GUID_SysMouse, &m_dimouse, NULL);
		if(FAILED(hr)) {
			return false;
		}

		// �f�[�^�t�H�[�}�b�g�̐ݒ�
		hr = m_dimouse->SetDataFormat(&c_dfDIMouse2);
		if(FAILED(hr)) {
			return false;
		}
		
		// �������x���̐ݒ�
		hr = m_dimouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		if(FAILED(hr)) {
			return false;
		}

		// �f�o�C�X�̐ݒ�
		DIPROPDWORD diprop;
		diprop.diph.dwSize = sizeof(diprop);
		diprop.diph.dwHeaderSize = sizeof(diprop.diph);
		diprop.diph.dwObj = 0;
		diprop.diph.dwHow = DIPH_DEVICE;
		diprop.dwData = DIPROPAXISMODE_REL;							// ���Βl���[�h
		m_dimouse->SetProperty(DIPROP_AXISMODE, &diprop.diph);		// �����[�h�̐ݒ�


		DIPROPRANGE diprg;
		diprg.diph.dwSize = sizeof(diprg);
		diprg.diph.dwHeaderSize = sizeof(diprg.diph);
		diprg.diph.dwObj = DIJOFS_X;
		diprg.diph.dwHow = DIPH_BYOFFSET;
		diprg.lMin = 0;
		diprg.lMax = width - 1;

		m_dimouse->SetProperty(DIPROP_RANGE, &diprg.diph);		// �w�����͈̔͂��w��
		diprg.diph.dwObj = DIJOFS_Y;
		diprg.diph.dwHow = DIPH_BYOFFSET;
		diprg.lMin = 0;
		diprg.lMax = height - 1;
		m_dimouse->SetProperty(DIPROP_RANGE, &diprg.diph);	// �x�����͈̔͂��w��

		m_hwnd = hwnd;

		m_height = height;
		m_width  = width;

		return true;
	}

	//----------------------------------
	// �}�E�X��Ԏ擾����
	//----------------------------------
	void GetMouseState(){
		HRESULT	hr;

		DIMOUSESTATE2		mouseStateOld = m_MouseState;

		GetCursorPos(&m_MousePoint);
		ScreenToClient(m_hwnd, &m_MousePoint);

		// �f�o�C�X�̔F��
		hr = m_dimouse->Acquire();

		hr = m_dimouse->GetDeviceState(sizeof(m_MouseState),&m_MouseState);
		if (SUCCEEDED(hr)){
			for (int cnt = 0; cnt < 8; cnt++)
			{
				m_MouseStateTrigger.rgbButtons[cnt] = ((mouseStateOld.rgbButtons[cnt] ^ m_MouseState.rgbButtons[cnt]) & m_MouseState.rgbButtons[cnt]);
			}
		}
		else{
			// 取得に失敗したフレームは前回の移動量が残るので0にする。
			// 残ったままだと、マウスを動かしていないのに視点が回り続ける。
			m_MouseState.lX = 0;
			m_MouseState.lY = 0;
			m_MouseState.lZ = 0;
			if(hr == DIERR_INPUTLOST){
				// �f�o�C�X�̔F��
				hr = m_dimouse->Acquire();
			}
		}	
	}

	//----------------------------------
	// �}�E�X�w���W�擾����
	//----------------------------------
	int GetMousePosX() const{
		return m_MousePoint.x;
	}

	//----------------------------------
	// �}�E�X�x���W�擾����
	//----------------------------------
	int GetMousePosY() const{
		return m_MousePoint.y;
	}

	int GetMouseWheelDelta() const {
		return m_MouseState.lZ;
	}
	int GetMouseDeltaX() const { return m_MouseState.lX; }
	int GetMouseDeltaY() const { return m_MouseState.lY; }

	//----------------------------------
	// �}�E�X���{�^���`�F�b�N
	//----------------------------------
	bool GetMouseLButtonCheck() const{
		if(m_MouseState.rgbButtons[0] & 0x80){
			return true;
		}else{
			return false;	
		}
	}

	//----------------------------------
	// �}�E�X�E�{�^���`�F�b�N
	//----------------------------------
	bool GetMouseRButtonCheck() const{
		if(m_MouseState.rgbButtons[1] & 0x80){
			return true;
		}else{
			return false;	
		}
	}

	//----------------------------------
	// �}�E�X�����{�^���`�F�b�N
	//----------------------------------
	bool GetMouseCButtonCheck() const{
		if(m_MouseState.rgbButtons[2] & 0x80){
			return true;
		}else{
			return false;	
		}
	}

	//----------------------------------
	// �}�E�X���{�^���`�F�b�N(�g���K�[)
	//----------------------------------
	bool GetMouseLButtonTrigger() const {
		if (m_MouseStateTrigger.rgbButtons[0] & 0x80) {
			return true;
		}
		else {
			return false;
		}
	}

	//----------------------------------
	// �}�E�X�E�{�^���`�F�b�N(�g���K�[)
	//----------------------------------
	bool GetMouseRButtonTrigger() const {
		if (m_MouseStateTrigger.rgbButtons[1] & 0x80) {
			return true;
		}
		else {
			return false;
		}
	}

	//----------------------------------
	// �}�E�X�����{�^���`�F�b�N(�g���K�[)
	//----------------------------------
	bool GetMouseCButtonTrigger() const {
		if (m_MouseStateTrigger.rgbButtons[2] & 0x80) {
			return true;
		}
		else {
			return false;
		}
	}

	//----------------------------------
	// �L�[�{�[�h�o�b�t�@�擾����
	//----------------------------------
	void GetKeyBuffer(){
		HRESULT	hr;
		// �f�o�C�X�̔F��
		hr = m_dikeyboard->Acquire();
		// �O��̏�Ԃ�ۑ�
		memcpy(&m_oldkeybuffer,m_keybuffer,sizeof(m_keybuffer));
		hr = m_dikeyboard->GetDeviceState(sizeof(m_keybuffer),(LPVOID)&m_keybuffer);
		if(hr == DIERR_INPUTLOST){
			// �f�o�C�X�̔F��
			hr = m_dikeyboard->Acquire();
		}
	}

	//----------------------------------
	// �L�[��������Ă��邩�ǂ������`�F�b�N����
	//		p1 :�@�`�F�b�N�������L�[�ԍ�
	//	�߂�l
	//		true : �w�肳�ꂽ�L�[��������Ă���
	//----------------------------------
	bool CheckKeyBuffer(int keyno){
		if(m_keybuffer[keyno] & 0x80){
			return true;
		}
		else{
			return false;
		}
	}

	//----------------------------------
	// �L�[��������Ă��邩�ǂ������`�F�b�N����
	//		p1 :�@�`�F�b�N�������L�[�ԍ�(�g���K�[)
	//	�߂�l
	//		true : �w�肳�ꂽ�L�[��������Ă���
	//----------------------------------
	bool CheckKeyBufferTrigger(int keyno){
		if(((m_keybuffer[keyno]^m_oldkeybuffer[keyno]) & m_keybuffer[keyno]) & 0x80){
			return true;
		}
		else{
			return false;
		}
	}

	//----------------------------------
	// DirectInput �I������
	//----------------------------------
	void Dispose(){
		if(m_dikeyboard!=nullptr){
			m_dikeyboard->Release();
		}
		if(m_dimouse!=nullptr){
			m_dimouse->Release();
		}
		if(m_dinput!=nullptr){
			m_dinput->Release();
		}
	}	

	//----------------------------------
	// �J�[�\���ʒu����ʒ����ɂ���
	//----------------------------------
	void SetCursorPosition() {

		int x = GetSystemMetrics(SM_CXSCREEN);
		int y = GetSystemMetrics(SM_CYSCREEN);
		SetForegroundWindow(m_hwnd);
		SetActiveWindow(m_hwnd);
		SetFocus(m_hwnd);
		SetCursorPos(x/2, y/2);
	}
};
