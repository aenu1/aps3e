// SPDX-License-Identifier: WTFPL

package aenu.aps3e;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.Toast;

import java.io.File;
import java.io.FileFilter;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.List;
import org.json.JSONObject;
import android.widget.Adapter;
import android.app.*;
import android.widget.*;
import java.util.*;
import android.content.*;
import android.icu.text.*;
import android.view.*;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

import aenu.view.SVListView;

public class KeyMapActivity extends AppCompatActivity {
    
	SharedPreferences sp;
    @Override
    public void onCreate(Bundle bundle) {

		supportRequestWindowFeature(Window.FEATURE_NO_TITLE);
		super.onCreate(bundle);
		Utils.setup_system_bars_appearance(this);
		sp=getSharedPreferences();
		update_config();
		setContentView(R.layout.activity_keymap);
		
		// 设置 Toolbar
		Toolbar toolbar = findViewById(R.id.toolbar);
		setSupportActionBar(toolbar);
		if(getSupportActionBar()!=null) {
			getSupportActionBar().setTitle(R.string.key_mappers);
		}
		
		((SVListView)findViewById(R.id.keymap_list)).setOnItemClickListener(click_l);
		((Button)findViewById(R.id.keymap_reset)).setOnClickListener(reset_l);
		((CheckBox)findViewById(R.id.enable_vibrator)).setChecked(sp.getBoolean("enable_vibrator",false));
		((CheckBox)findViewById(R.id.enable_vibrator)).setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener(){
			@Override
			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				sp.edit().putBoolean("enable_vibrator",isChecked).commit();
			}
		});
		/*
		final String vibrator_duration_hiht=getString(R.string.vibrator_duration)+":  ";
		((TextView)findViewById(R.id.vibrator_duration_label)).setText(vibrator_duration_hiht+sp.getInt("vibrator_duration",25));
		((SeekBar)findViewById(R.id.vibrator_duration)).setProgress(sp.getInt("vibrator_duration",25));
		((SeekBar)findViewById(R.id.vibrator_duration)).setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
			@Override
			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				((TextView)findViewById(R.id.vibrator_duration_label)).setText(vibrator_duration_hiht+progress);
			}
			@Override
			public void onStartTrackingTouch(SeekBar seekBar) {}
			@Override
			public void onStopTrackingTouch(SeekBar seekBar) {}
		});*/

		refresh_view();
    }
	
	void refresh_view(){
		((SVListView)findViewById(R.id.keymap_list)).setAdapter(new KeyListAdapter(this,KeyMapConfig.KEY_NAMEIDS,KeyMapConfig.KEY_IDS,get_all_key_mapper_values()));
	}
	
	void update_config(){
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(KeyMapActivity.this);
		SharedPreferences.Editor sPrefsEditor = sPrefs.edit();

		for(int i=0;i<KeyMapConfig.KEY_IDS.length;i++){
			String key=Integer.toString(KeyMapConfig.KEY_IDS[i]);
			int default_v=KeyMapConfig.DEFAULT_KEYMAPPERS[i];
			int key_v=sPrefs.getInt(key,default_v);
			sPrefsEditor.putInt(key,key_v);
		}
		
		sPrefsEditor.commit();
	}
	
	int[] get_all_key_mapper_values(){
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(KeyMapActivity.this);
		
		int[] key_values=new int[KeyMapConfig.KEY_IDS.length];
		for(int i=0;i<KeyMapConfig.KEY_IDS.length;i++){
			String key_n=Integer.toString(KeyMapConfig.KEY_IDS[i]);
			key_values[i]=sPrefs.getInt(key_n,0);
		}
		return key_values;
	}
	
	private final AdapterView.OnItemClickListener click_l=new AdapterView.OnItemClickListener(){
		@Override
		public void onItemClick(final AdapterView<?> l, View v, final int position,long id)
		{
			AlertDialog.Builder builder = new AlertDialog.Builder(KeyMapActivity.this);
            builder.setMessage(R.string.press_a_key);
            builder.setNegativeButton(R.string.clear, new DialogInterface.OnClickListener(){
					@Override
					public void onClick(DialogInterface p1, int p2)
					{
						final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(KeyMapActivity.this);
						SharedPreferences.Editor sPrefsEditor = sPrefs.edit();
						sPrefsEditor.putInt((String)l.getItemAtPosition(position),0);
						sPrefsEditor.commit();
						refresh_view();
					}
				});
			builder.setOnKeyListener(new DialogInterface.OnKeyListener(){
					@Override
					public boolean onKey(DialogInterface dialog, int keyCode, KeyEvent event)
					{
						if (event.getAction() == KeyEvent.ACTION_DOWN) {
							final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(KeyMapActivity.this);
							SharedPreferences.Editor sPrefsEditor = sPrefs.edit();
							sPrefsEditor.putInt((String)l.getItemAtPosition(position),keyCode);
							sPrefsEditor.commit();
							dialog.dismiss();
							refresh_view();
							return true;
						}
						return false;
					}
			});
            AlertDialog dialog = builder.create();
            dialog.show();  
		}
	};

	private final View.OnClickListener reset_l=new View.OnClickListener(){
		@Override
		public void onClick(View v)
		{
			for(int i=0;i<KeyMapConfig.KEY_IDS.length;i++){
				String key_n=Integer.toString(KeyMapConfig.KEY_IDS[i]);
				int default_v=KeyMapConfig.DEFAULT_KEYMAPPERS[i];
				sp.edit().putInt(key_n,default_v).commit();
			}
			refresh_view();
		}
	};

    private SharedPreferences getSharedPreferences() {
        return PreferenceManager.getDefaultSharedPreferences(this);
    }
	
	private static class KeyListAdapter extends BaseAdapter {

        private int[] keyNameIdList_;
		private int[] keyIdList_;
		private int[] valueList_;
        private Context context_; 

        private KeyListAdapter(Context context,int[] keyNameList,int[] keyList,int[] valueList){
            context_=context;
			this.keyNameIdList_=keyNameList;
			this.keyIdList_=keyList;
			this.valueList_=valueList;
		}

        public String getKey(int pos){
            return Integer.toString(keyIdList_[pos]);
        }

		public String getKeyName(int pos){
            return context_.getString(keyNameIdList_[pos]);
        }

        @Override
        public int getCount(){
            return keyNameIdList_.length;
        }

        @Override
        public Object getItem(int p1){
            return getKey(p1);
        }

        @Override
        public long getItemId(int p1){
            return p1;
        }

        @Override
        public View getView(int pos,View curView,ViewGroup p3){

            
            if(curView==null){
                curView=new TextView(context_,null,androidx.appcompat.R.attr.textAppearanceListItem);
            }
			
			TextView text=(TextView)curView;

            text.setText(getKeyName(pos)+":    "+valueList_[pos]);

            return curView;
        } 
    }//!FileAdapter
}
