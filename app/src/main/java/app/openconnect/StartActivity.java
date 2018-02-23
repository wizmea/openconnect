package app.openconnect;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import app.openconnect.core.OpenVpnService;
import app.openconnect.core.VPNConnector;
import app.openconnect.fragments.FeedbackFragment;

/**
 * Created by arcadius on 11/02/2018.
 */

public class StartActivity extends Activity {

    private VPNConnector mConn;
    private TextView txt;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.start_activity);

        txt = (TextView) findViewById(R.id.status);

        final OpenConnectHelper helper = new OpenConnectHelper(this);


        findViewById(R.id.vpn).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                helper.setUserCredential("ectest","Abcd1234").startVPN();
            }
        });
        
        FeedbackFragment.recordUse(this, false);

    }

    @Override
    protected void onResume() {
        super.onResume();

        mConn = new VPNConnector(this, true) {
            @Override
            public void onUpdate(OpenVpnService service) {
                service.startActiveDialog(StartActivity.this);
                txt.setText(service.getConnectionStateName());
                //updateUI(service);
            }
        };
    }

    @Override
    protected void onPause() {
        mConn.stopActiveDialog();
        mConn.unbind();
        super.onPause();
    }

    private void stopVPN(){
        mConn.service.stopVPN();
    }
}
