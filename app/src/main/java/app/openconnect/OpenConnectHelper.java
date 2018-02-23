package app.openconnect;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import app.openconnect.api.GrantPermissionsActivity;
import app.openconnect.core.ProfileManager;
import app.openconnect.fragments.FeedbackFragment;

/**
 * Created by arcadius on 11/02/2018.
 */

public class OpenConnectHelper {

    private Context activity;
    private String PREFNAME = "openConnectGate";

    public OpenConnectHelper(Activity activity) {
        this.activity = activity;
    }

    public OpenConnectHelper(Context activity) {
        this.activity = activity;
    }

    public OpenConnectHelper setUserCredential(String username,String password){
        SharedPreferences p = activity.getSharedPreferences(PREFNAME, Activity.MODE_PRIVATE);
        p.edit().putString("username", username).apply();
        p.edit().putString("password", password).apply();
        return this;
    }

    public String getUsername(){
        SharedPreferences p = activity.getSharedPreferences(PREFNAME, Activity.MODE_PRIVATE);
        return p.getString("username","");
    }

    public String getPassword(){
        SharedPreferences p = activity.getSharedPreferences(PREFNAME, Activity.MODE_PRIVATE);
        return p.getString("password","");
    }

    //start vpn
    public void startVPN() {
        VpnProfile profile = ProfileManager.create("gate.presidence.bj");
        Intent intent = new Intent(activity, GrantPermissionsActivity.class);
        String pkg = activity.getPackageName();

        intent.putExtra(pkg + GrantPermissionsActivity.EXTRA_UUID, profile.getUUID().toString());
        intent.setAction(Intent.ACTION_MAIN);
        activity.startActivity(intent);
    }

}
