#include "LedMatrixApp.h"
#include <cinder/MayaCamUI.h>
#include "LedState.h"

void LedMatrixApp::mouseDown( MouseEvent event )
{
	maya_cam->mouseDown( event.getPos() );
}

void LedMatrixApp::mouseDrag( MouseEvent event )
{
	maya_cam->mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown());
}

void LedMatrixApp::keyUp( KeyEvent event )
{
	static StateType idle_states[] = {T_0, T_1, T_SPARK,T_LOTS};
	static StateType interative_states[] = {T_ANIMAL, T_FOLLOWING, T_SPARK_INT};
	int key_id = -1;

	switch (event.getCode())
	{
	case KeyEvent::KEY_r:
		{
			reloadConfig();
		}break;
	case KeyEvent::KEY_SPACE:
		{
			show_3d = !show_3d;
		}break;
	case KeyEvent::KEY_1:
		{
			key_id = 0;
		}break;
	case KeyEvent::KEY_2:
		{
			key_id = 1;
		}break;
	case KeyEvent::KEY_3:
		{
			key_id = 2;
		}break;	
	case KeyEvent::KEY_4:
		{
			key_id = 3;
		}break;
	case  KeyEvent::KEY_ESCAPE:
		{
			if (IDYES == ::MessageBox(NULL, L"Sure to exit?", L"LedMatrix", MB_YESNO))
				quit();
		}
	default:
		break;
	}


	bool idle = LedState::isIdleState(current_states[0]->_type);
	if (idle)
	{
		if (key_id >= 0 && key_id <= 3)
			changeToState(LedState::create(*this, 0, idle_states[key_id]));
	}
	else
	{
		if (key_id >= 0 && key_id <= 2)
			changeToState(LedState::create(*this, 0, interative_states[key_id]));
	}
}
