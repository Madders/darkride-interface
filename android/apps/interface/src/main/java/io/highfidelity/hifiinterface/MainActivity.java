package io.highfidelity.hifiinterface;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import androidx.annotation.NonNull;
import com.google.android.material.navigation.NavigationView;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;
import androidx.core.view.GravityCompat;
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.TextView;

import com.squareup.picasso.Callback;
import com.squareup.picasso.MemoryPolicy;
import com.squareup.picasso.Picasso;

import io.highfidelity.hifiinterface.fragment.FriendsFragment;
import io.highfidelity.hifiinterface.fragment.HomeFragment;
import io.highfidelity.hifiinterface.fragment.PolicyFragment;
import io.highfidelity.hifiinterface.fragment.ProfileFragment;
import io.highfidelity.hifiinterface.fragment.SettingsFragment;
import io.highfidelity.hifiinterface.fragment.SignupFragment;
import io.highfidelity.hifiinterface.theme.ConsoleBars;
import io.highfidelity.hifiinterface.theme.Theme;
import io.highfidelity.hifiinterface.theme.ThemeManager;
import io.highfidelity.hifiinterface.theme.ThemeStyler;
import io.highfidelity.hifiinterface.theme.ThemeWalker;

public class MainActivity extends AppCompatActivity implements NavigationView.OnNavigationItemSelectedListener,
                                                                HomeFragment.OnHomeInteractionListener,
                                                                FriendsFragment.OnHomeInteractionListener,
                                                                ProfileFragment.OnProfileInteractionListener {

    private static final int PROFILE_PICTURE_PLACEHOLDER = R.drawable.default_profile_avatar;
    public static final String DEFAULT_FRAGMENT = "Home";
    public static final String EXTRA_FRAGMENT = "fragment";
    public static final String EXTRA_BACK_TO_SCENE = "backToScene";
    public static final String EXTRA_BACK_TO_URL = "url";

    private String TAG = "HighFidelity";

    public native void logout();
    public native void setUsernameChangedListener(Activity usernameChangedListener);
    public native String getUsername();

    private DrawerLayout mDrawerLayout;
    private Toolbar mToolbar;
    private Theme mTheme;
    // the bars in the console layout (themes like Cyber HUD), or null
    private ConsoleBars mConsoleBars;
    private NavigationView mNavigationView;
    private ImageView mProfilePicture;
    private TextView mDisplayName;
    private View mLoginPanel;
    private View mProfilePanel;
    private TextView mLogoutOption;
    private MenuItem mPeopleMenuItem;
    private MenuItem mProfileMenuItem;

    private boolean backToScene;
    private String backToUrl;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        MenuOrientation.apply(this);
        ThemeWalker.themeFragments(this, getFragmentManager());
        mTheme = ThemeManager.get(this);
        boolean console = Theme.LAYOUT_CONSOLE.equals(mTheme.layout);
        setContentView(console ? R.layout.activity_main_console : R.layout.activity_main);

        mNavigationView = findViewById(R.id.nav_view);
        mNavigationView.setNavigationItemSelectedListener(this);

        mLoginPanel = mNavigationView.getHeaderView(0).findViewById(R.id.loginPanel);
        mProfilePanel = mNavigationView.getHeaderView(0).findViewById(R.id.profilePanel);

        // (inside the drawer in portrait, under the rail in landscape)
        mLogoutOption = findViewById(R.id.logout);

        mDisplayName = mNavigationView.getHeaderView(0).findViewById(R.id.displayName);
        mProfilePicture = mNavigationView.getHeaderView(0).findViewById(R.id.profilePicture);

        mPeopleMenuItem = mNavigationView.getMenu().findItem(R.id.action_people);

        mProfileMenuItem = mNavigationView.getMenu().findItem(R.id.action_profile);

        updateDebugMenu(mNavigationView.getMenu());

        Toolbar toolbar = findViewById(R.id.toolbar);
        toolbar.setTitleTextAppearance(this, R.style.HomeActionBarTitleStyle);
        setSupportActionBar(toolbar);

        // (in landscape the menu is a rail that's always there, not a drawer: layout-land/activity_main.xml)
        mDrawerLayout = findViewById(R.id.drawer_layout);

        ActionBar actionbar = getSupportActionBar();
        actionbar.setDisplayHomeAsUpEnabled(mDrawerLayout != null);
        actionbar.setHomeAsUpIndicator(R.drawable.ic_menu);

        Window window = getWindow();
        window.clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        window.setStatusBarColor(ContextCompat.getColor(this, R.color.statusbar_color));

        mToolbar = toolbar;
        ThemeManager.publishToEngine(this);
        ThemeStyler.styleMainScreen(this, mTheme, ((android.view.ViewGroup) findViewById(android.R.id.content)).getChildAt(0),
                toolbar, mNavigationView);
        ThemeStyler.styleDisplay(mDisplayName, mTheme);
        mDisplayName.setTextColor(mTheme.text);
        ThemeStyler.styleMenuOption(mLogoutOption, mTheme);
        ThemeStyler.styleMenuOption(findViewById(R.id.policy), mTheme);
        if (console) {
            mConsoleBars = new ConsoleBars(this, mTheme, new ConsoleBars.Listener() {
                @Override
                public void onSectionSelected(MenuItem item) {
                    onNavigationItemSelected(item);
                }

                @Override
                public void onProfileClicked(View anchor) {
                    showProfileMenu(anchor);
                }
            });
        }

        if (savedInstanceState != null) {
            // turned round: the fragments are put back by themselves
            FragmentManager fragmentManager = getFragmentManager();
            int count = fragmentManager.getBackStackEntryCount();
            if (count > 0) {
                setTitle(fragmentManager.getBackStackEntryAt(count - 1).getName());
            }
            backToScene = savedInstanceState.getBoolean(EXTRA_BACK_TO_SCENE, false);
            backToUrl = getIntent() != null ? getIntent().getStringExtra(EXTRA_BACK_TO_URL) : null;
        } else if (getIntent() != null) {
            if (getIntent().hasExtra(EXTRA_FRAGMENT)) {
                loadFragment(getIntent().getStringExtra(EXTRA_FRAGMENT));
            } else {
                loadFragment(DEFAULT_FRAGMENT);
            }

            backToScene = getIntent().getBooleanExtra(EXTRA_BACK_TO_SCENE, false);
            backToUrl = getIntent().getStringExtra(EXTRA_BACK_TO_URL);
        }
    }

    private void updateDebugMenu(Menu menu) {
        if (BuildConfig.DEBUG) {
            for (int i=0; i < menu.size(); i++) {
                if (menu.getItem(i).getItemId() == R.id.action_debug_settings) {
                    menu.getItem(i).setVisible(true);
                }
            }
        }
    }

    private void loadFragment(String fragment) {
        switch (fragment) {
            case "Home":
                loadHomeFragment(true);
                break;
            case "Privacy Policy":
                loadPrivacyPolicyFragment();
                break;
            case "People":
                loadPeopleFragment();
                break;
            default:
                Log.e(TAG, "Unknown fragment " + fragment);
        }

    }

    private void loadHomeFragment(boolean addToBackStack) {
        Fragment fragment = HomeFragment.newInstance();
        loadFragment(fragment, getString(R.string.home), getString(R.string.tagFragmentHome), addToBackStack, true);
    }

    private void startLoginMenuActivity() {
        Intent intent = new Intent(this, LoginMenuActivity.class);
        intent.putExtra(LoginMenuActivity.EXTRA_BACK_ON_SKIP, true);
        startActivity(intent);
    }

    private void loadPrivacyPolicyFragment() {
        Fragment fragment = PolicyFragment.newInstance();

        loadFragment(fragment, getString(R.string.privacyPolicy), getString(R.string.tagFragmentPolicy), true, true);
    }

    private void loadPeopleFragment() {
        Fragment fragment = FriendsFragment.newInstance();

        loadFragment(fragment, getString(R.string.people), getString(R.string.tagFragmentPeople), true, true);
    }

    private void loadProfileFragment() {
        Fragment fragment = ProfileFragment.newInstance();

        loadFragment(fragment, getString(R.string.profile), getString(R.string.tagFragmentProfile), true, true);
    }

    private void loadSettingsFragment() {
        SettingsFragment fragment = SettingsFragment.newInstance();

        loadFragment(fragment, getString(R.string.settings), getString(R.string.tagSettings), true, true);
    }


    private void loadFragment(Fragment newFragment, String title, String tag, boolean addToBackStack, boolean goBackUntilHome) {
        FragmentManager fragmentManager = getFragmentManager();

        // check if it's the same fragment
        String currentFragmentName = fragmentManager.getBackStackEntryCount() > 0
                ? fragmentManager.getBackStackEntryAt(fragmentManager.getBackStackEntryCount() - 1).getName()
                : "";
        if (currentFragmentName.equals(title)) {
            closeDrawer();
            return; // cancel as we are already in that fragment
        }

        if (goBackUntilHome) {
            // go back until first transaction
            int backStackEntryCount = fragmentManager.getBackStackEntryCount();
            for (int i = 0; i < backStackEntryCount - 1; i++) {
                fragmentManager.popBackStackImmediate();
            }
        }

        // this case is when we wanted to go home.. rollback already did that!
        // But asking for a new Home fragment makes it easier to have an updated list so we let it to continue

        FragmentTransaction ft = fragmentManager.beginTransaction();
        ft.replace(R.id.content_frame, newFragment, tag);

        if (addToBackStack) {
            ft.addToBackStack(title);
        }
        ft.commit();
        setTitle(title);
        closeDrawer();
    }

    private void closeDrawer() {
        if (mDrawerLayout != null) {
            mDrawerLayout.closeDrawer(mNavigationView);
        }
    }

    
    private void updateLoginMenu() {
        if (HifiUtils.getInstance().isUserLoggedIn()) {
            mLoginPanel.setVisibility(View.GONE);
            mProfilePanel.setVisibility(View.VISIBLE);
            mLogoutOption.setVisibility(View.VISIBLE);
            mPeopleMenuItem.setVisible(true);
            updateProfileHeader();
            if (mConsoleBars != null) {
                mConsoleBars.setUser(getUsername(), true);
            }
        } else {
            mLoginPanel.setVisibility(View.VISIBLE);
            mProfilePanel.setVisibility(View.GONE);
            mLogoutOption.setVisibility(View.GONE);
            mPeopleMenuItem.setVisible(false);
            mDisplayName.setText("");
            if (mConsoleBars != null) {
                mConsoleBars.setUser(null, false);
                mConsoleBars.setProfilePicture(getDrawable(PROFILE_PICTURE_PLACEHOLDER));
            }
        }
        if (mConsoleBars != null) {
            mConsoleBars.setSections(mNavigationView.getMenu());
        }
    }

    private void updateProfileHeader() {
        updateProfileHeader(getUsername());
    }
    private void updateProfileHeader(String username) {
        if (!username.isEmpty()) {
            mDisplayName.setText(username);
            updateProfilePicture(username);
        }
    }

    // Screen titles in the theme's style
    @Override
    public void setTitle(CharSequence title) {
        if (mTheme == null) {
            super.setTitle(title);
            return;
        }
        super.setTitle(ThemeStyler.title(mTheme, title));
        mToolbar.post(() -> ThemeStyler.styleToolbarTitle(mToolbar, mTheme));
        if (mConsoleBars != null) {
            mConsoleBars.setTitle(title);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        //getMenuInflater().inflate(R.menu.menu_navigation, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();
        switch (id) {
            case android.R.id.home:
                if (mDrawerLayout != null) {
                    mDrawerLayout.openDrawer(GravityCompat.START);
                }
                return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onNavigationItemSelected(@NonNull MenuItem item) {
        switch(item.getItemId()) {
            case R.id.action_home:
                loadHomeFragment(false);
                return true;
            case R.id.action_people:
                loadPeopleFragment();
                return true;
            case R.id.action_profile:
                loadProfileFragment();
                break;
            case R.id.action_debug_settings:
                loadSettingsFragment();
                return true;
        }
        return false;
    }

    @Override
    protected void onStart() {
        super.onStart();
        setUsernameChangedListener(this);
        updateLoginMenu();
    }

    @Override
    protected void onStop() {
        super.onStop();
        setUsernameChangedListener(null);
    }

    // The console layout's profile: log in, or log out, and the privacy policy
    private void showProfileMenu(View anchor) {
        android.widget.PopupMenu menu = new android.widget.PopupMenu(this, anchor);
        boolean loggedIn = HifiUtils.getInstance().isUserLoggedIn();
        menu.getMenu().add(0, 1, 0, loggedIn ? R.string.logout : R.string.login);
        menu.getMenu().add(0, 2, 1, R.string.privacyPolicy);
        menu.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == 1) {
                if (loggedIn) {
                    onLogoutClicked(anchor);
                } else {
                    startLoginMenuActivity();
                }
            } else {
                loadPrivacyPolicyFragment();
            }
            return true;
        });
        menu.show();
    }

    public void onLoginClicked(View view) {
        startLoginMenuActivity();
    }

    public void onLogoutClicked(View view) {
        logout();
        updateLoginMenu();
        exitLoggedInFragment();

    }

    private void exitLoggedInFragment() {
        // If we are in a "logged in" fragment (like People), go back to home. This could be expanded to multiple fragments
        FragmentManager fragmentManager = getFragmentManager();
        String currentFragmentName = fragmentManager.getBackStackEntryCount() > 0
                ? fragmentManager.getBackStackEntryAt(fragmentManager.getBackStackEntryCount() - 1).getName()
                : "";
        if (currentFragmentName.equals(getString(R.string.people))) {
            loadHomeFragment(false);
        }
    }

    public void onSelectedDomain(String domainUrl) {
        goToDomain(domainUrl);
    }

    private void goToLastLocation() {
        goToDomain(backToUrl != null? backToUrl : "");
    }

    private void goToDomain(String domainUrl) {
        Intent intent = new Intent(this, InterfaceActivity.class);
        intent.putExtra(InterfaceActivity.DOMAIN_URL, domainUrl);
        finish();
        intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        startActivity(intent);
        rabbitHole();
    }

    private void goToUser(String username) {
        Intent intent = new Intent(this, InterfaceActivity.class);
        intent.putExtra(InterfaceActivity.EXTRA_GOTO_USERNAME, username);
        finish();
        intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        startActivity(intent);
        rabbitHole();
    }

    // Into a world, in a Wonderland theme: down the rabbit hole
    private void rabbitHole() {
        if (mTheme != null && mTheme.whimsy) {
            overridePendingTransition(R.anim.rabbit_hole_in, R.anim.rabbit_hole_out);
        }
    }

    public void handleUsernameChanged(String username) {
        runOnUiThread(() -> updateProfileHeader(username));
    }

    // the picture set on the metaverse, fetched afresh each time so a change there shows up
    private void updateProfilePicture(String username) {
        mProfilePicture.setImageResource(PROFILE_PICTURE_PLACEHOLDER);
        Picasso.get().load(HifiUtils.userImageUrl(username))
                .memoryPolicy(MemoryPolicy.NO_CACHE, MemoryPolicy.NO_STORE)
                .placeholder(PROFILE_PICTURE_PLACEHOLDER)
                .into(mProfilePicture, new RoundProfilePictureCallback());
    }

    public void onPrivacyPolicyClicked(View view) {
        loadPrivacyPolicyFragment();
    }

    @Override
    public void onVisitUserSelected(String username) {
        goToUser(username);
    }

    @Override
    public void onCancelProfileEdit() {
        loadHomeFragment(false);
    }

    @Override
    public void onCompleteProfileEdit() {
        loadHomeFragment(false);
    }

    @Override
    public void onAvatarChosen() {
        loadHomeFragment(false);
    }

    private class RoundProfilePictureCallback implements Callback {
        @Override
        public void onSuccess() {
            Bitmap imageBitmap = ((BitmapDrawable) mProfilePicture.getDrawable()).getBitmap();
            RoundedBitmapDrawable imageDrawable = RoundedBitmapDrawableFactory.create(getResources(), imageBitmap);
            imageDrawable.setCircular(true);
            imageDrawable.setCornerRadius(Math.max(imageBitmap.getWidth(), imageBitmap.getHeight()) / 2.0f);
            mProfilePicture.setImageDrawable(imageDrawable);
            if (mConsoleBars != null) {
                mConsoleBars.setProfilePicture(imageDrawable);
            }
        }

        @Override
        public void onError(Exception e) {
            mProfilePicture.setImageResource(PROFILE_PICTURE_PLACEHOLDER);
        }
    }

    @Override
    public void onBackPressed() {
        // if a fragment needs to internally manage back presses..
        FragmentManager fm = getFragmentManager();
        Fragment friendsFragment = fm.findFragmentByTag(getString(R.string.tagFragmentPeople));
        if (friendsFragment != null && friendsFragment instanceof FriendsFragment) {
            if (((FriendsFragment) friendsFragment).onBackPressed()) {
                return;
            }
        }

        int index = fm.getBackStackEntryCount() - 1;

        if (index > 0) {
            super.onBackPressed();
            index--;
            if (index > -1) {
                setTitle(fm.getBackStackEntryAt(index).getName());
            }
            if (backToScene) {
                backToScene = false;
                goToLastLocation();
            }
        } else {
            finishAffinity();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle savedInstanceState) {
        super.onSaveInstanceState(savedInstanceState);
        savedInstanceState.putBoolean(EXTRA_BACK_TO_SCENE, backToScene);
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        backToScene = savedInstanceState.getBoolean(EXTRA_BACK_TO_SCENE, false);
    }
}
