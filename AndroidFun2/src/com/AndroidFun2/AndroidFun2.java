
package com.AndroidFun2;

import android.app.Activity;
import android.widget.TextView;
import android.os.Bundle;
import org.libcinder.app.CinderNativeActivity;

public class AndroidFun2 extends CinderNativeActivity
{
    static final String TAG = "AndroidFun2";
    static {
        System.loadLibrary("hello-jni");
    }
}
