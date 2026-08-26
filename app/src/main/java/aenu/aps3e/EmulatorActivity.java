// SPDX-License-Identifier: WTFPL

package aenu.aps3e;

import android.app.*;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.*;
import android.view.*;
import android.content.*;
import android.widget.*;
import android.preference.*;
import android.util.*;
import org.vita3k.emulator.overlay.InputOverlay.ControlId;
import android.content.res.*;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.fragment.app.FragmentManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.lang.Process;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

import aenu.hardware.ProcessorInfo;

//import org.libsdl.app.*;

public class EmulatorActivity extends AppCompatActivity {
	//{ System.loadLibrary("e"); }

	public static final String EXTRA_META_INFO = "meta_info";
	public static final String EXTRA_ISO_URI = "iso_uri";
	public static final String EXTRA_GAME_DIR = "game_dir";
	public static final String EXTRA_SAVESTATE_PATH = "savestate_path";

	private static final String PREF_LAST_SESSION_ACTIVE = "last_session_active";
	private static final String PREF_LAST_SESSION_END = "last_session_end_time";
	private static final String PREF_LAST_SESSION_START = "last_session_start_time";
	private static final String PREF_LAST_GAME_SERIAL = "last_game_serial";

	private static final String FRAGMENT_TAG_DELAY_DIALOG = "delay_dialog";
	private static final String FRAGMENT_TAG_MENU_DIALOG = "menu_dialog";
	private static final String FRAGMENT_TAG_CLOSING_DIALOG = "closing_dialog";
	private static final String FRAGMENT_TAG_MEMORY_SEARCH_DIALOG = "memory_search_dialog";

	private static final int WHAT_DELAY_ON_CREATE = 0xaeae0001;
	private static final int WHAT_FINISHING = 0xaeae0002;

	private static final String FRAGMENT_TAG_MAIN =  "main";

	Emulator.MetaInfo meta_info = null;

	static class XHandler extends Handler {
		final WeakReference<EmulatorActivity> activity_ref;
		XHandler(EmulatorActivity activity) {
			activity_ref = new WeakReference<>(activity);
		}
		@Override
		public void handleMessage(@NonNull Message msg) {

			EmulatorActivity activity = activity_ref.get();
			if(activity == null||activity.isFinishing()) return;

			switch (msg.what){
				case WHAT_DELAY_ON_CREATE:{
					DialogFragment delay_dialog = (DialogFragment)activity.getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG_DELAY_DIALOG);
					if (delay_dialog != null) {
						delay_dialog.dismiss();
					}
					activity.on_create();
				}break;
				case WHAT_FINISHING:{
					DialogFragment closing_dialog = (DialogFragment)activity.getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG_CLOSING_DIALOG);
					if (closing_dialog != null) {
						closing_dialog.dismiss();
					}
					activity.finish();
				}break;
			}


		}
	};
	final Handler handler=new XHandler(this);

	static class XOnBackPressedCallback extends OnBackPressedCallback {

		final WeakReference<EmulatorActivity> activity_ref;
		public XOnBackPressedCallback(EmulatorActivity activity,boolean enabled) {
			super(enabled);
			activity_ref = new WeakReference<>(activity);
		}

		@Override
		public void handleOnBackPressed() {
			EmulatorActivity activity = activity_ref.get();
			if(activity == null||activity.isFinishing()) return;
			DialogFragment dialog;
			dialog = (DialogFragment)activity.getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG_DELAY_DIALOG);
			if (dialog != null) return;
			dialog = (DialogFragment)activity.getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG_CLOSING_DIALOG);
			if (dialog != null) return;

			dialog = (DialogFragment)activity.getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG_MENU_DIALOG);
			if (dialog == null) {
				dialog = DialogFragment.newInstance(FRAGMENT_TAG_MENU_DIALOG);
				dialog.show(activity.getSupportFragmentManager(), FRAGMENT_TAG_MENU_DIALOG);
				return;
			} else {
				dialog.dismiss();
				return;
			}
		}
	}

	final XOnBackPressedCallback emu_back_pressed_callback = new XOnBackPressedCallback(this,true);


	boolean mem_searched = false;
	Emulator.CheatInfo[] mem_search_results;

	void on_create() {

		//get meta info
		{
			try {
				if ((meta_info = (Emulator.MetaInfo) getIntent().getSerializableExtra("meta_info")) != null) {

					if (meta_info.eboot_path == null && meta_info.iso_uri != null){
						meta_info.iso_fd = Utils.detach_open_uri(this, Uri.parse(meta_info.iso_uri));
						meta_info.dec_key_fd = Utils.try_open_key_fd(this, Uri.parse(meta_info.iso_uri));
					}

				} else {
					String iso_uri = getIntent().getStringExtra(EXTRA_ISO_URI);
					String game_dir = getIntent().getStringExtra(EXTRA_GAME_DIR);
					if (iso_uri != null) {
						int dec_key_fd = Utils.try_open_key_fd(this, Uri.parse(iso_uri));
						meta_info = Emulator.get.meta_info_from_iso(Utils.detach_open_uri(this, Uri.parse(iso_uri)),dec_key_fd, iso_uri);
						meta_info.iso_fd = Utils.detach_open_uri(this, Uri.parse(iso_uri));
						meta_info.dec_key_fd = dec_key_fd;
					}
					if (game_dir != null) {
						meta_info = Emulator.get.meta_info_from_dir(game_dir);
					}

					if (meta_info == null)
						throw new RuntimeException("Failed to get meta info");
				}
			} catch (Exception e) {
				Toast.makeText(EmulatorActivity.this, e.getMessage(), Toast.LENGTH_SHORT).show();
			}
		}

		//setenv
		{
			File custom_cfg = Application.get_custom_cfg_file(meta_info.serial);
			if (custom_cfg.exists())
				Emulator.get.set_env("APS3E_CUSTOM_CONFIG_YAML_PATH", custom_cfg.getAbsolutePath());

			boolean enable_log = getSharedPreferences("debug", MODE_PRIVATE).getBoolean("enable_log", false);
			Emulator.get.set_env("APS3E_ENABLE_LOG", Boolean.toString(enable_log));

			String savestate_path=getIntent().getStringExtra(EXTRA_SAVESTATE_PATH);
			if (savestate_path!=null && !savestate_path.isEmpty())
				Emulator.get.set_env("APS3E_LOAD_SAVESTATE_PATH", savestate_path);
		}

		//setup game path
		{
			if (meta_info.eboot_path != null)
				Emulator.get.setup_game_path(meta_info.eboot_path);
			else if (meta_info.iso_uri != null)
				Emulator.get.setup_game_path(aenu.emulator.Emulator.Path.from(meta_info.iso_uri, meta_info.iso_fd, meta_info.dec_key_fd));
			else
				throw new RuntimeException("Failed to get meta info");
		}

		Emulator.get.setup_game_id(meta_info.serial);

		//view
		setContentView(R.layout.activity_emulator);

		EmulatorFragment emulator_fragment = (EmulatorFragment) getSupportFragmentManager().findFragmentByTag(
				FRAGMENT_TAG_MAIN);
		if (emulator_fragment == null) {
			emulator_fragment = EmulatorFragment.newInstance(meta_info);
			getSupportFragmentManager()
					.beginTransaction()
					.add(R.id.fragment_emulator,emulator_fragment,FRAGMENT_TAG_MAIN)
					.commit();
		}
	}

	void delay_on_create() {
		DialogFragment delay_dialog = DialogFragment.newInstance(FRAGMENT_TAG_DELAY_DIALOG);
		delay_dialog.show(getSupportFragmentManager(),FRAGMENT_TAG_DELAY_DIALOG);

		new Thread() {
			@Override
			public void run() {
				try {
					Thread.sleep(500);
					Emulator.load_library();
					Thread.sleep(100);
					handler.sendEmptyMessage(WHAT_DELAY_ON_CREATE);
				} catch (InterruptedException e) {
					throw new RuntimeException(e);
				}
			}
		}.start();
	}

	void show_memory_search_view() {
		DialogFragment dialog = DialogFragment.newInstance(FRAGMENT_TAG_MEMORY_SEARCH_DIALOG);
		dialog.show(getSupportFragmentManager(), FRAGMENT_TAG_MEMORY_SEARCH_DIALOG);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {

		supportRequestWindowFeature(Window.FEATURE_NO_TITLE);

		super.onCreate(savedInstanceState);
		Utils.enable_fullscreen(getWindow());
		getOnBackPressedDispatcher().addCallback(emu_back_pressed_callback);

		if (!Application.should_delay_load()) {
			on_create();
			return;
		}

		delay_on_create();
		return;
	}

	@Override
	protected void onStart() {
		super.onStart();
	}

	void close(){
		DialogFragment menu_dialog = (DialogFragment)getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG_MENU_DIALOG);
		if (menu_dialog != null) menu_dialog.dismiss();
		DialogFragment closing_dialog = DialogFragment.newInstance(FRAGMENT_TAG_CLOSING_DIALOG);
		closing_dialog.show(getSupportFragmentManager(), FRAGMENT_TAG_CLOSING_DIALOG);

		new Thread() {
			@Override
			public void run() {
				try {
					Emulator.get.quit();
					handler.sendEmptyMessage(EmulatorActivity.WHAT_FINISHING);
				} catch (Exception e) {
					throw new RuntimeException(e);
				}
			}
		}.start();
	}

	void show_main_memu() {
		DialogFragment menu_dialog = DialogFragment.newInstance(FRAGMENT_TAG_MENU_DIALOG);
		menu_dialog.show(getSupportFragmentManager(), FRAGMENT_TAG_MENU_DIALOG);
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		System.exit(0);
	}

	public static class DialogFragment extends androidx.fragment.app.DialogFragment{
		private static final String KEY_DIALOG_TAG = "dialog_type";
		public static DialogFragment newInstance(String dialog_tag) {
			Bundle args = new Bundle();
			args.putString(KEY_DIALOG_TAG, dialog_tag);
			DialogFragment fragment = new DialogFragment();
			fragment.setArguments(args);
			return fragment;
		}

		static void update_mem_search_view(Context ctx,Emulator.CheatInfo[] searchResults,ListView searchListView,TextView searchResultTextView){

			final ArrayAdapter<String> listAdapter = new ArrayAdapter<>(ctx, android.R.layout.simple_list_item_1);
			if (searchResults == null || searchResults.length == 0) {
				listAdapter.clear();
				searchResultTextView.setText(R.string.search_results_0_matches);
				//Toast.makeText(getContext(), R.string.no_matching_memory_address_found, Toast.LENGTH_SHORT).show();
				searchListView.setAdapter(listAdapter);
				return;
			}
			listAdapter.clear();
			if (searchResults.length <= 50) {
				for (Emulator.CheatInfo info : searchResults) {
					listAdapter.add(String.format("0x%08X", info.addr));
				}
			}
			searchListView.setAdapter(listAdapter);
			searchResultTextView.setText(String.format(ctx.getString(R.string.search_results_n_matches), searchResults.length));
		}
		@NonNull
		@Override
		public  Dialog onCreateDialog(Bundle savedInstanceState) {
			switch (getArguments().getString(KEY_DIALOG_TAG)) {
				case FRAGMENT_TAG_DELAY_DIALOG: {
					return ProgressTask.create_progress_dialog(getContext(), getString(R.string.loading));
				}
				case FRAGMENT_TAG_MENU_DIALOG: {
					AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
					ViewGroup contents = (ViewGroup) getLayoutInflater().inflate(
							R.layout.dialog_running_menu, null);

					//setup contents
					{
						TextView title = contents.findViewById(R.id.text_title);
						TextView serial = contents.findViewById(R.id.text_serial);

						Emulator.MetaInfo meta_info = ((EmulatorActivity)getActivity()).meta_info;
						if (meta_info != null) {
							title.setText(meta_info.name);
							serial.setText(meta_info.serial);
						}

						final String[] options = new String[]{
								getString(R.string.memory_search),
								getString(R.string.quit),
						};

						ListView list = contents.findViewById(R.id.list_options);
						list.setAdapter(new ArrayAdapter<>(getContext(), android.R.layout.simple_list_item_1, options));
						list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
							@Override
							public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
								if (parent.getItemAtPosition(position).equals(getString(R.string.memory_search))) {
									((EmulatorActivity) getActivity()).show_memory_search_view();
								} else if (parent.getItemAtPosition(position).equals(getString(R.string.quit))) {
									((EmulatorActivity) getActivity()).close();
								} else {
									throw new RuntimeException("unknown option");
								}
							}
						});
					}

					builder.setView(contents);
					return builder.create();
				}
				case FRAGMENT_TAG_MEMORY_SEARCH_DIALOG: {
					AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
					View view = getLayoutInflater().inflate(R.layout.memory_search_view, null);
					builder.setView(view);

					Spinner searchTypeSpinner = (Spinner) view.findViewById(R.id.search_type);
					EditText searchValueEditText = (EditText) view.findViewById(R.id.search_value);
					TextView searchResultTextView = (TextView) view.findViewById(R.id.search_result);
					ListView searchListView = (ListView) view.findViewById(R.id.search_list);
					TextView selectedAddressTextView = (TextView) view.findViewById(R.id.selected_address);
					EditText writeValueEditText = (EditText) view.findViewById(R.id.write_value);
					Button btnSearch = (Button) view.findViewById(R.id.btn_search);
					Button btnReset = (Button) view.findViewById(R.id.btn_reset);
					Button btnWrite = (Button) view.findViewById(R.id.btn_write);


					final String[] strSearchTypes = {"u32", "u16", "u8"};
					final int[] searchTypes = {Emulator.CheatInfo.TYPE_U32, Emulator.CheatInfo.TYPE_U16, Emulator.CheatInfo.TYPE_U8};
					ArrayAdapter<String> adapter = new ArrayAdapter<>(getContext(), android.R.layout.simple_spinner_item, strSearchTypes);
					adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
					searchTypeSpinner.setAdapter(adapter);

					EmulatorActivity activity = (EmulatorActivity) getActivity();
					if(activity.mem_searched&&activity.mem_search_results!=null&&activity.mem_search_results.length!=0){
						Emulator.CheatInfo cheatInfo=activity.mem_search_results[0];
						for(int i=0;i<searchTypes.length;i++){
							if(cheatInfo.type==searchTypes[i]){
								searchTypeSpinner.setSelection(i);
								break;
							}
						}
						searchTypeSpinner.setEnabled(false);

						update_mem_search_view(getContext(),activity.mem_search_results,searchListView,searchResultTextView);
					}


					btnSearch.setOnClickListener(new View.OnClickListener() {
						@Override
						public void onClick(View v) {
							String searchValueStr = searchValueEditText.getText().toString();
							if (searchValueStr.isEmpty()) {
								Toast.makeText(getContext(), R.string.please_enter_a_search_value, Toast.LENGTH_SHORT).show();
								return;
							}

							try {
								long searchValue = Utils.convert_hex_str_to_long(searchValueStr);
								int searchType = searchTypes[searchTypeSpinner.getSelectedItemPosition()];
								String exception_msg_prefix = getString(R.string.search_value_cannot_be_greater_than);
								switch (searchType) {
									case Emulator.CheatInfo.TYPE_U8:
										if (searchValue > 0xFF)
											throw new Exception(exception_msg_prefix + "0xFF");
										break;

									case Emulator.CheatInfo.TYPE_U16:
										if (searchValue > 0xFFFF)
											throw new Exception(exception_msg_prefix + "0xFFFF");
										break;

									case Emulator.CheatInfo.TYPE_U32:
										if (searchValue > 0xFFFFFFFFL)
											throw new Exception(exception_msg_prefix + "0xFFFFFFFF");
										break;
								}

								Emulator.CheatInfo searchInfo = new Emulator.CheatInfo();
								searchInfo.type = searchType;
								searchInfo.value = searchValue;

								Emulator.CheatInfo[] searchResults;
								if(!activity.mem_searched){
									searchResults=activity.mem_search_results=Emulator.get.search_memory(searchInfo);
									activity.mem_searched=true;
								}
								else{
									searchResults=activity.mem_search_results;
									if(searchResults!=null&&searchResults.length>0){
										ArrayList<Emulator.CheatInfo> filteredResults = new ArrayList<>();
										for (Emulator.CheatInfo info : searchResults) {
											Emulator.get.get_cheat(info);
											if (info.value == searchValue) {
												filteredResults.add(info);
											}
										}
										searchResults = filteredResults.toArray(new Emulator.CheatInfo[0]);
										activity.mem_search_results=searchResults;
									}
								}

								if(searchResults==null||searchResults.length==0){
									selectedAddressTextView.setText(R.string.please_select_the_address_to_write_to_first);
								}

								searchTypeSpinner.setEnabled(false);
								update_mem_search_view(getContext(),searchResults,searchListView,searchResultTextView);

								Toast.makeText(getContext(), String.format(getString(R.string.search_results_n_matches), searchResults.length), Toast.LENGTH_SHORT).show();

							} catch (NumberFormatException e) {
								Toast.makeText(getContext(), R.string.please_enter_a_valid_number, Toast.LENGTH_SHORT).show();
							} catch (Exception e) {
								Toast.makeText(getContext(), getString(R.string.search_failed) + e.getMessage(), Toast.LENGTH_LONG).show();
							}
						}
					});

					btnReset.setOnClickListener(new View.OnClickListener() {
						@Override
						public void onClick(View v) {
							EmulatorActivity activity = (EmulatorActivity) getActivity();
							activity.mem_searched=false;
							activity.mem_search_results=null;

							searchTypeSpinner.setEnabled(true);
							searchListView.setAdapter(new ArrayAdapter<>(getContext(), android.R.layout.simple_list_item_1));
							searchResultTextView.setText("");
							selectedAddressTextView.setText(R.string.please_select_the_memory_address_to_write_to_first);
							searchValueEditText.setText("");
							writeValueEditText.setText("");
						}
					});

					searchListView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
						@Override
						public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
							String selectedItem = (String) parent.getAdapter().getItem(position);
							if (selectedItem != null) {
								selectedAddressTextView.setText(getString(R.string.address_selected) + selectedItem);
							}
						}
					});

					btnWrite.setOnClickListener(new View.OnClickListener() {
						@Override
						public void onClick(View v) {
							String selectedAddressStr = selectedAddressTextView.getText().toString();
							if (selectedAddressStr.split("0x").length != 2) {
								Toast.makeText(getContext(), R.string.please_select_the_memory_address_to_write_to_first, Toast.LENGTH_SHORT).show();
								return;
							}
							selectedAddressStr = selectedAddressStr.split("0x")[1];

							String writeValueStr = writeValueEditText.getText().toString();
							if (writeValueStr.isEmpty()) {
								Toast.makeText(getContext(), R.string.please_enter_the_value_to_write, Toast.LENGTH_SHORT).show();
								return;
							}

							try {
								long address = Long.parseLong(selectedAddressStr.toUpperCase(), 16);
								long writeValue = Utils.convert_hex_str_to_long(writeValueStr);
								int writeType = searchTypes[searchTypeSpinner.getSelectedItemPosition()];
								String exception_msg_prefix = getString(R.string.the_value_to_be_written_cannot_be_greater_than);

								switch (writeType) {
									case Emulator.CheatInfo.TYPE_U8:
										if (writeValue > 0xFF)
											throw new Exception(exception_msg_prefix + "0xFF");
										break;

									case Emulator.CheatInfo.TYPE_U16:
										if (writeValue > 0xFFFF)
											throw new Exception(exception_msg_prefix + "0xFFFF");
										break;

									case Emulator.CheatInfo.TYPE_U32:
										if (writeValue > 0xFFFFFFFFL)
											throw new Exception(exception_msg_prefix + "0xFFFFFFFF");
										break;
								}

								Emulator.CheatInfo writeInfo = new Emulator.CheatInfo();
								writeInfo.type = writeType;
								writeInfo.addr = address;
								writeInfo.value = writeValue;

								Emulator.get.set_cheat(writeInfo);

								Toast.makeText(getContext(), getString(R.string.write_successful) + ": 0x" + Long.toHexString(address) + " = " + writeValue, Toast.LENGTH_SHORT).show();

							} catch (Exception e) {
								Toast.makeText(getContext(), getString(R.string.write_failed) + ": " + e.getMessage(), Toast.LENGTH_LONG).show();
							}
						}
					});
					return builder.create();
				}
				case FRAGMENT_TAG_CLOSING_DIALOG: {
					return ProgressTask.create_progress_dialog(getContext(), null);
				}
				default:
					throw new IllegalArgumentException();
			}
        }
	}

	public static class EmulatorFragment extends androidx.fragment.app.Fragment implements SurfaceHolder.Callback
			,View.OnGenericMotionListener
	,View.OnKeyListener{

		private static final String KEY_GAMEINFO = "game_info";
		public static EmulatorFragment newInstance(Emulator.MetaInfo meta_info) {
			Bundle args = new Bundle();
			//Serializable
			args.putSerializable(KEY_GAMEINFO, meta_info);
			EmulatorFragment fragment = new EmulatorFragment();
			fragment.setArguments(args);
			return fragment;
		}

		private SparseIntArray keys_map = new SparseIntArray();
		private Vibrator vibrator = null;
		private VibrationEffect vibration_effect = null;

		boolean started = false;
		SurfaceView gv;
		@Override
		public View onCreateView(LayoutInflater inflater, ViewGroup container,
								 Bundle savedInstanceState) {
			return inflater.inflate(R.layout.fragment_emulator, container, false);
		}

		@Override
		public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
			super.onViewCreated(view, savedInstanceState);
			gv = (GameFrameView) view.findViewById(R.id.game_frame);

			gv.setFocusable(true);
			gv.setFocusableInTouchMode(true);
			gv.requestFocus();

			gv.setOnGenericMotionListener(this);
			gv.setOnKeyListener(this);
			gv.getHolder().addCallback(this);

			load_key_map_and_vibrator();
		}
		@Override
		public void onPause() {
			super.onPause();

			if (started)
				if (Emulator.get.is_running())
					Emulator.get.pause();
		}

		@Override
		public void onResume() {
			super.onResume();

			if (started && gv.getHolder().getSurface().isValid() && Emulator.get.is_paused()) {
				Emulator.get.resume();
			}

		}

		@Override
		public void onConfigurationChanged(@NonNull Configuration newConfig) {
			super.onConfigurationChanged(newConfig);
		}

		void load_key_map_and_vibrator() {
			final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(getContext());
			keys_map.clear();
			for (int i = 0; i < KeyMapConfig.KEY_IDS.length; i++) {
				String key = Integer.toString(KeyMapConfig.KEY_IDS[i]);
				int keyCode = sPrefs.getInt(key, KeyMapConfig.DEFAULT_KEYMAPPERS[i]);
				keys_map.put(keyCode, KeyMapConfig.KEY_VALUES[i]);
			}
			if (sPrefs.getBoolean("enable_vibrator", false)) {
				vibrator = (Vibrator) getContext().getSystemService(VIBRATOR_SERVICE);
				vibration_effect = VibrationEffect.createOneShot(25, VibrationEffect.DEFAULT_AMPLITUDE);
			}
		}

		@Override
		public void surfaceCreated(SurfaceHolder holder) {
			if (!started) {
				started = true;
				try {
					Emulator.get.setup_surface(holder.getSurface());
				} finally {
					try {
						Emulator.get.boot();
					} catch (Emulator.BootException e) {
						throw new RuntimeException(e);
					}
				}

			} else {

				Emulator.get.setup_surface(holder.getSurface());

				if (Emulator.get.is_paused())
					Emulator.get.resume();
			}
		}

		@Override
		public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
			if (!started) return;
			if (width == 0 || height == 0) return;
			Emulator.get.change_surface(width, height);
		}

		@Override
		public void surfaceDestroyed(SurfaceHolder holder) {
			if (!started)
				return;
			Emulator.get.setup_surface(null);
		}

		boolean handle_dpad(InputEvent event) {

			boolean pressed = false;
			//dpad和摇杆都是MotionEvent
			if (event instanceof MotionEvent) {

				// Use the hat axis value to find the D-pad direction
				MotionEvent motionEvent = (MotionEvent) event;
				float xaxis = motionEvent.getAxisValue(MotionEvent.AXIS_HAT_X);
				float yaxis = motionEvent.getAxisValue(MotionEvent.AXIS_HAT_Y);

				// Check if the AXIS_HAT_X value is -1 or 1, and set the D-pad
				// LEFT and RIGHT direction accordingly.
				if (Float.compare(xaxis, -1.0f) == 0) {
					Emulator.get.key_event(ControlId.l, true);
					Emulator.get.key_event(ControlId.r, false);
					vibrator();
					pressed = true;
				} else if (Float.compare(xaxis, 1.0f) == 0) {
					Emulator.get.key_event(ControlId.r, true);
					Emulator.get.key_event(ControlId.l, false);

					vibrator();
					pressed = true;
				} else {
					Emulator.get.key_event(ControlId.l, false);
					Emulator.get.key_event(ControlId.r, false);
				}
				// Check if the AXIS_HAT_Y value is -1 or 1, and set the D-pad
				// UP and DOWN direction accordingly.
				if (Float.compare(yaxis, -1.0f) == 0) {
					Emulator.get.key_event(ControlId.u, true);
					Emulator.get.key_event(ControlId.d, false);

					vibrator();
					pressed = true;
				} else if (Float.compare(yaxis, 1.0f) == 0) {
					Emulator.get.key_event(ControlId.d, true);
					Emulator.get.key_event(ControlId.u, false);

					vibrator();
					pressed = true;
				} else {
					Emulator.get.key_event(ControlId.u, false);
					Emulator.get.key_event(ControlId.d, false);
				}
			}
		/*else if (event instanceof KeyEvent) {

			// Use the key code to find the D-pad direction.
            KeyEvent keyEvent = (KeyEvent) event;
            if (keyEvent.getKeyCode() == KeyEvent.KEYCODE_DPAD_LEFT) {
                Emulator.get.key_event(ControlId.l, true);
				Emulator.get.key_event(ControlId.r, false);

				vibrator();
				pressed=true;

            } else if (keyEvent.getKeyCode() == KeyEvent.KEYCODE_DPAD_RIGHT) {
                Emulator.get.key_event(ControlId.r, true);
				Emulator.get.key_event(ControlId.l, false);

				vibrator();
				pressed=true;

            } else if (keyEvent.getKeyCode() == KeyEvent.KEYCODE_DPAD_UP) {
                Emulator.get.key_event(ControlId.u, true);
				Emulator.get.key_event(ControlId.d, false);

				vibrator();
				pressed=true;

            } else if (keyEvent.getKeyCode() == KeyEvent.KEYCODE_DPAD_DOWN) {
                Emulator.get.key_event(ControlId.d, true);
				Emulator.get.key_event(ControlId.u, false);

				vibrator();
				pressed=true;

            }
		}*/

			if (pressed) return true;
			Emulator.get.key_event(ControlId.l, false);
			Emulator.get.key_event(ControlId.u, false);
			Emulator.get.key_event(ControlId.r, false);
			Emulator.get.key_event(ControlId.d, false);
			return false;
		}


		private static boolean isDpadDevice(MotionEvent event) {
			// Check that input comes from a device with directional pads.
			if ((event.getSource() & InputDevice.SOURCE_DPAD)
					!= InputDevice.SOURCE_DPAD) {
				return true;
			} else {
				return false;
			}
		}

		@Override
		public boolean onGenericMotion(View v, MotionEvent event) {

			if (isDpadDevice(event) && handle_dpad(event)) return true;

			if ((event.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK/*&&
			event.getAction() == MotionEvent.ACTION_MOVE*/) {
				float laxisX = event.getAxisValue(MotionEvent.AXIS_X);
				float laxisY = event.getAxisValue(MotionEvent.AXIS_Y);
				float raxisX = event.getAxisValue(MotionEvent.AXIS_Z);
				float raxisY = event.getAxisValue(MotionEvent.AXIS_RZ);

				//左摇杆
				{
					if (laxisX != 0) {
						if (laxisX < 0) {
							Emulator.get.key_event(ControlId.lsr, false);
							Emulator.get.key_event(ControlId.lsl, true, (int) (Math.abs(laxisX) * 255.0));
						} else {
							Emulator.get.key_event(ControlId.lsl, false);
							Emulator.get.key_event(ControlId.lsr, true, (int) (Math.abs(laxisX) * 255.0));
						}
					} else {
						Emulator.get.key_event(ControlId.lsr, false);
						Emulator.get.key_event(ControlId.lsl, false);
					}

					if (laxisY != 0) {
						if (laxisY < 0) {
							Emulator.get.key_event(ControlId.lsd, false);
							Emulator.get.key_event(ControlId.lsu, true, (int) (Math.abs(laxisY) * 255.0));
						} else {
							Emulator.get.key_event(ControlId.lsu, false);
							Emulator.get.key_event(ControlId.lsd, true, (int) (Math.abs(laxisY) * 255.0));
						}
					} else {
						Emulator.get.key_event(ControlId.lsd, false);
						Emulator.get.key_event(ControlId.lsu, false);
					}
				}
				//右摇杆
				{
					if (raxisX != 0) {
						if (raxisX < 0) {
							Emulator.get.key_event(ControlId.rsr, false);
							Emulator.get.key_event(ControlId.rsl, true, (int) (Math.abs(raxisX) * 255.0));
						} else {
							Emulator.get.key_event(ControlId.rsl, false);
							Emulator.get.key_event(ControlId.rsr, true, (int) (Math.abs(raxisX) * 255.0));
						}
					} else {
						Emulator.get.key_event(ControlId.rsr, false);
						Emulator.get.key_event(ControlId.rsl, false);
					}

					if (raxisY != 0) {
						if (raxisY < 0) {
							Emulator.get.key_event(ControlId.rsd, false);
							Emulator.get.key_event(ControlId.rsu, true, (int) (Math.abs(raxisY) * 255.0));
						} else {
							Emulator.get.key_event(ControlId.rsu, false);
							Emulator.get.key_event(ControlId.rsd, true, (int) (Math.abs(raxisY) * 255.0));
						}
					} else {
						Emulator.get.key_event(ControlId.rsd, false);
						Emulator.get.key_event(ControlId.rsu, false);
					}
				}
				return true;
			}

			return false;
		}

		void vibrator() {
			if (vibrator != null) {
				vibrator.vibrate(vibration_effect);
			}
		}

		@Override
		public boolean onKey(View v, int keyCode, KeyEvent event) {
			int gameKey = keys_map.get(keyCode, 0);
			if (gameKey == 0) return false;
			if(event.getAction()==KeyEvent.ACTION_DOWN){
				if (event.getRepeatCount() == 0) {
					vibrator();
					Emulator.get.key_event(gameKey, true);
					return true;
				}
			}
			else if(event.getAction()==KeyEvent.ACTION_UP){
				Emulator.get.key_event(gameKey, false);
				return true;
			}

			return false;
		}
	}
}
