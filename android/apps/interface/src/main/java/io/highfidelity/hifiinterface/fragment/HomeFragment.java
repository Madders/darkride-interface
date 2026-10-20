package io.highfidelity.hifiinterface.fragment;

import android.app.Fragment;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.app.AlertDialog;
import android.net.Uri;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ImageView;
import android.widget.TextView;

import io.highfidelity.hifiinterface.HifiUtils;
import io.highfidelity.hifiinterface.R;
import io.highfidelity.hifiinterface.theme.BackgroundDrawable;
import io.highfidelity.hifiinterface.theme.Theme;
import io.highfidelity.hifiinterface.theme.ThemeManager;
import io.highfidelity.hifiinterface.theme.ThemeStyler;
import io.highfidelity.hifiinterface.provider.Bookmarks;
import io.highfidelity.hifiinterface.view.BookmarkAdapter;
import io.highfidelity.hifiinterface.view.DomainAdapter;
import androidx.recyclerview.widget.LinearLayoutManager;
import com.google.android.material.tabs.TabLayout;

public class HomeFragment extends Fragment {

    private DomainAdapter mDomainAdapter;
    private RecyclerView mDomainsView;
    private TextView searchNoResultsView;
    private ImageView mSearchIconView;
    private ImageView mClearSearch;
    private EditText mSearchView;
    private TextView mGoToAddressView;
    private LinearLayout mGoToAddressRow;
    private ImageView mSaveBookmarkButton;
    private TabLayout mTabs;
    private TextView mLastLocationButton;
    private RecyclerView mBookmarksView;
    private BookmarkAdapter mBookmarkAdapter;
    private Bookmarks mBookmarks;
    private String mLastLocation;
    private boolean mShowingBookmarks;
    private boolean mDirectoryEmpty;

    private static final String PREFERENCES = "places";
    private static final String PREFERENCE_TAB = "tab";
    private static final int TAB_DIRECTORY = 0;
    private static final int TAB_BOOKMARKS = 1;


    private OnHomeInteractionListener mListener;
    private SwipeRefreshLayout mSwipeRefreshLayout;

    public native String nativeGetLastLocation();

    public HomeFragment() {
        // Required empty public constructor
    }

    public static HomeFragment newInstance() {
        HomeFragment fragment = new HomeFragment();
        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_home, container, false);

        searchNoResultsView = rootView.findViewById(R.id.searchNoResultsView);
        mSwipeRefreshLayout = rootView.findViewById(R.id.swipeRefreshLayout);

        mDomainsView = rootView.findViewById(R.id.rvDomains);
        // as many columns as fit (one in portrait, two or more in landscape)
        int numberOfColumns = Math.max(1, (int) (getContentWidthPx() / getResources().getDimension(R.dimen.domainMinColumnWidth)));
        GridLayoutManager gridLayoutMgr = new GridLayoutManager(getContext(), numberOfColumns);
        mDomainsView.setLayoutManager(gridLayoutMgr);
        mLastLocation = nativeGetLastLocation();
        mDomainAdapter = new DomainAdapter(getContext(), HifiUtils.getInstance().protocolVersionSignature(), mLastLocation);
        mSwipeRefreshLayout.setRefreshing(true);
        mDomainAdapter.setClickListener((view, position, domain) -> {
            new Handler(getActivity().getMainLooper()).postDelayed(() -> {
                if (mListener != null) {
                    mListener.onSelectedWorld(domain.url, domain.name, domain.author, domain.thumbnail);
                }
            }, 400); // a delay so the ripple effect can be seen
        });
        mDomainAdapter.setListener(new DomainAdapter.AdapterListener() {
            @Override
            public void onEmptyAdapter(boolean shouldStopRefreshing) {
                mDirectoryEmpty = true;
                if (!mShowingBookmarks) {
                    showNoResults();
                    searchNoResultsView.setVisibility(View.VISIBLE);
                    mDomainsView.setVisibility(View.GONE);
                }
                if (shouldStopRefreshing) {
                    mSwipeRefreshLayout.setRefreshing(false);
                }
            }

            @Override
            public void onNonEmptyAdapter(boolean shouldStopRefreshing) {
                mDirectoryEmpty = false;
                if (!mShowingBookmarks) {
                    searchNoResultsView.setVisibility(View.GONE);
                    mDomainsView.setVisibility(View.VISIBLE);
                }
                if (shouldStopRefreshing) {
                    mSwipeRefreshLayout.setRefreshing(false);
                }
            }

            @Override
            public void onError(Exception e, String message) {
                // stop the spinner, so pulling down can try again
                mSwipeRefreshLayout.setRefreshing(false);
                if (mDomainAdapter.getItemCount() == 0 && !mShowingBookmarks) {
                    searchNoResultsView.setText(R.string.domains_load_error);
                    searchNoResultsView.setVisibility(View.VISIBLE);
                    mDomainsView.setVisibility(View.GONE);
                }
            }
        });
        mDomainsView.setAdapter(mDomainAdapter);
        mDomainAdapter.startLoad();

        mSearchView = rootView.findViewById(R.id.searchView);
        mSearchIconView = rootView.findViewById(R.id.search_mag_icon);
        mClearSearch = rootView.findViewById(R.id.search_clear);
        mGoToAddressView = rootView.findViewById(R.id.goToAddressView);
        mGoToAddressRow = rootView.findViewById(R.id.goToAddressRow);
        mSaveBookmarkButton = rootView.findViewById(R.id.saveBookmarkButton);
        mTabs = rootView.findViewById(R.id.placesTabs);
        mLastLocationButton = rootView.findViewById(R.id.lastLocationButton);
        mBookmarksView = rootView.findViewById(R.id.rvBookmarks);

        getActivity().getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);

        Theme theme = ThemeManager.get(getContext());
        rootView.setBackground(new BackgroundDrawable(theme, getResources().getDisplayMetrics().density));
        ThemeStyler.styleField(mSearchView, theme);
        ThemeStyler.styleField(mGoToAddressView, theme);
        mGoToAddressView.setTextColor(theme.primary);
        mSearchIconView.setColorFilter(theme.icon);
        mClearSearch.setColorFilter(theme.icon);
        ThemeStyler.styleBody(searchNoResultsView, theme);
        searchNoResultsView.setTextColor(theme.textSecondary);
        mSwipeRefreshLayout.setColorSchemeColors(theme.primary, theme.accent);
        mSwipeRefreshLayout.setProgressBackgroundColorSchemeColor(theme.surface);
        mSaveBookmarkButton.setColorFilter(theme.primary);

        // Bookmarks: worlds saved by address on this phone
        mBookmarks = new Bookmarks(getContext());
        mBookmarkAdapter = new BookmarkAdapter(theme, new BookmarkAdapter.Listener() {
            @Override
            public void onBookmarkClicked(Bookmarks.Bookmark bookmark) {
                if (mListener != null) {
                    mListener.onSelectedWorld(HifiUtils.getInstance().sanitizeHifiUrl(bookmark.address), bookmark.name, "", "");
                }
            }

            @Override
            public void onBookmarkLongPressed(Bookmarks.Bookmark bookmark) {
                showBookmarkOptions(bookmark);
            }
        });
        mBookmarksView.setLayoutManager(new LinearLayoutManager(getContext()));
        mBookmarksView.setAdapter(mBookmarkAdapter);

        // The directory or the bookmarks, as last shown
        mTabs.addTab(mTabs.newTab().setText(R.string.places_tab_directory));
        mTabs.addTab(mTabs.newTab().setText(R.string.places_tab_bookmarks));
        mTabs.setTabTextColors(theme.textSecondary, theme.primary);
        mTabs.setSelectedTabIndicatorColor(theme.primary);
        int tab = getContext().getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE).getInt(PREFERENCE_TAB, TAB_DIRECTORY);
        TabLayout.Tab selected = mTabs.getTabAt(tab == TAB_BOOKMARKS ? TAB_BOOKMARKS : TAB_DIRECTORY);
        if (selected != null) {
            selected.select();
        }
        showTab(tab == TAB_BOOKMARKS);
        mTabs.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                getContext().getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE).edit()
                        .putInt(PREFERENCE_TAB, tab.getPosition()).apply();
                showTab(tab.getPosition() == TAB_BOOKMARKS);
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {}

            @Override
            public void onTabReselected(TabLayout.Tab tab) {}
        });

        // Back to where you were: small, at the end of the tabs' row, where it was a whole world's card
        if (mLastLocation != null && !mLastLocation.isEmpty()) {
            ThemeStyler.styleBody(mLastLocationButton, theme);
            mLastLocationButton.setTextColor(theme.primary);
            android.graphics.drawable.Drawable icon = getContext().getDrawable(R.drawable.ic_last_location).mutate();
            icon.setTint(theme.primary);
            mLastLocationButton.setCompoundDrawablesWithIntrinsicBounds(icon, null, null, null);
            mLastLocationButton.setVisibility(View.VISIBLE);
        }

        return rootView;
    }

    @Override
    public void onStart() {
        super.onStart();
        mSearchView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void afterTextChanged(Editable editable) {
                mDomainAdapter.loadDomains(editable.toString(), false);
                String address = editable.toString().trim();
                if (looksLikeAddress(address)) {
                    mGoToAddressView.setText(getString(R.string.go_to_address, address));
                    mGoToAddressRow.setVisibility(View.VISIBLE);
                } else {
                    mGoToAddressRow.setVisibility(View.GONE);
                }
                if (mShowingBookmarks) {
                    showBookmarks();
                }
                if (editable.length() > 0) {
                    mSearchIconView.setVisibility(View.GONE);
                    mClearSearch.setVisibility(View.VISIBLE);
                } else {
                    mSearchIconView.setVisibility(View.VISIBLE);
                    mClearSearch.setVisibility(View.GONE);
                }
            }
        });
        // Enter on a hardware keyboard, or the on-screen keyboard's Go: an address is joined; a name is just searched
        mSearchView.setOnKeyListener((view, i, keyEvent) -> {
            if (i == KeyEvent.KEYCODE_ENTER && keyEvent.getAction() == KeyEvent.ACTION_UP) {
                onSearchSubmitted();
                return true;
            }
            return i == KeyEvent.KEYCODE_ENTER;
        });
        mSearchView.setOnEditorActionListener((view, actionId, keyEvent) -> {
            if (actionId == EditorInfo.IME_ACTION_GO || actionId == EditorInfo.IME_ACTION_DONE) {
                onSearchSubmitted();
                return true;
            }
            return false;
        });
        mGoToAddressView.setOnClickListener(view -> goToAddress(mSearchView.getText().toString().trim()));
        mSaveBookmarkButton.setOnClickListener(view -> askBookmarkName(mSearchView.getText().toString().trim(), null));
        mLastLocationButton.setOnClickListener(view -> {
            if (mListener != null) {
                mListener.onSelectedDomain(mLastLocation);
            }
        });

        mClearSearch.setOnClickListener(view -> onSearchClear(view));

        mSwipeRefreshLayout.setOnRefreshListener(new SwipeRefreshLayout.OnRefreshListener() {
            @Override
            public void onRefresh() {
                mDomainAdapter.loadDomains(mSearchView.getText().toString(), true);
            }
        });
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        if (context instanceof OnHomeInteractionListener) {
            mListener = (OnHomeInteractionListener) context;
        } else {
            throw new RuntimeException(context.toString()
                    + " must implement OnHomeInteractionListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mListener = null;
    }

    // The screen's width less the landscape menu rail's
    private float getContentWidthPx() {
        float width = getResources().getDisplayMetrics().widthPixels;
        if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE
                && !Theme.LAYOUT_CONSOLE.equals(ThemeManager.get(getContext()).layout)) {
            width -= getResources().getDimension(R.dimen.menuRailWidth);
        }
        return width;
    }

    // No places found: in a Wonderland theme, under the Cheshire Cat's grin
    private void showNoResults() {
        Theme theme = ThemeManager.get(getContext());
        if (theme.whimsy) {
            searchNoResultsView.setText(R.string.search_no_results_whimsy);
            android.graphics.drawable.Drawable grin = getContext().getDrawable(R.drawable.ic_cheshire_grin).mutate();
            grin.setTint(theme.primary);
            searchNoResultsView.setCompoundDrawablesWithIntrinsicBounds(null, grin, null, null);
            searchNoResultsView.setCompoundDrawablePadding((int) (12 * getResources().getDisplayMetrics().density));
            searchNoResultsView.setGravity(android.view.Gravity.CENTER);
        } else {
            searchNoResultsView.setText(R.string.search_no_results);
            searchNoResultsView.setCompoundDrawables(null, null, null, null);
        }
    }

    // An address rather than a place's name: a URL (hifi://, or any scheme), a host with a dot, an IP, a port or a path
    // (hifi://host:port/x,y,z/orientation), with no spaces, which names have and addresses don't
    static boolean looksLikeAddress(String text) {
        if (text.isEmpty() || text.matches(".*\\s.*")) {
            return false;
        }
        return text.contains("://") || text.contains(".") || text.contains(":") || text.contains("/");
    }

    private void onSearchSubmitted() {
        String text = mSearchView.getText().toString().trim();
        if (looksLikeAddress(text)) {
            goToAddress(text);
        } else {
            // the list already shows the matches: put the keyboard away to see them
            InputMethodManager keyboard = (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            if (keyboard != null) {
                keyboard.hideSoftInputFromWindow(mSearchView.getWindowToken(), 0);
            }
        }
    }

    private void goToAddress(String address) {
        if (address.isEmpty() || mListener == null) {
            return;
        }
        mListener.onSelectedDomain(HifiUtils.getInstance().sanitizeHifiUrl(address));
    }

    // The directory's list, or the bookmarks', with what each shows when it has nothing
    private void showTab(boolean bookmarks) {
        mShowingBookmarks = bookmarks;
        mSwipeRefreshLayout.setVisibility(bookmarks ? View.GONE : View.VISIBLE);
        if (bookmarks) {
            showBookmarks();
        } else {
            mBookmarksView.setVisibility(View.GONE);
            if (mDirectoryEmpty) {
                showNoResults();
                searchNoResultsView.setVisibility(View.VISIBLE);
                mDomainsView.setVisibility(View.GONE);
            } else {
                searchNoResultsView.setVisibility(View.GONE);
                mDomainsView.setVisibility(View.VISIBLE);
            }
        }
    }

    private void showBookmarks() {
        String filter = mSearchView != null ? mSearchView.getText().toString() : "";
        java.util.List<Bookmarks.Bookmark> all = mBookmarks.getAll();
        mBookmarkAdapter.show(all, filter);
        boolean empty = mBookmarkAdapter.getItemCount() == 0;
        mBookmarksView.setVisibility(empty ? View.GONE : View.VISIBLE);
        if (empty) {
            searchNoResultsView.setCompoundDrawables(null, null, null, null);
            searchNoResultsView.setText(all.isEmpty() ? R.string.bookmarks_empty : R.string.bookmarks_no_match);
            searchNoResultsView.setVisibility(View.VISIBLE);
        } else {
            searchNoResultsView.setVisibility(View.GONE);
        }
    }

    // A name for an address, to save it (or, given the bookmark, to rename it): the world's host to start with
    private void askBookmarkName(String address, Bookmarks.Bookmark existing) {
        if (address.isEmpty()) {
            return;
        }
        final EditText nameField = new EditText(getContext());
        nameField.setSingleLine(true);
        nameField.setHint(R.string.bookmark_name_hint);
        nameField.setText(existing != null ? existing.name : suggestedName(address));
        nameField.setSelectAllOnFocus(true);
        int padding = (int) (20 * getResources().getDisplayMetrics().density);
        android.widget.FrameLayout frame = new android.widget.FrameLayout(getContext());
        frame.setPadding(padding, padding / 2, padding, 0);
        frame.addView(nameField);
        new AlertDialog.Builder(getContext())
                .setTitle(R.string.bookmark_name_title)
                .setView(frame)
                .setPositiveButton(R.string.bookmark_save, (dialog, which) -> {
                    String name = nameField.getText().toString().trim();
                    mBookmarks.save(name.isEmpty() ? suggestedName(address) : name, address);
                    TabLayout.Tab tab = mTabs.getTabAt(TAB_BOOKMARKS);
                    if (tab != null && !tab.isSelected()) {
                        tab.select();
                    } else {
                        showBookmarks();
                    }
                })
                .setNegativeButton(R.string.bookmark_cancel, null)
                .show();
        nameField.requestFocus();
    }

    private void showBookmarkOptions(Bookmarks.Bookmark bookmark) {
        String[] options = { getString(R.string.bookmark_rename), getString(R.string.bookmark_delete) };
        new AlertDialog.Builder(getContext())
                .setTitle(bookmark.name)
                .setItems(options, (dialog, which) -> {
                    if (which == 0) {
                        askBookmarkName(bookmark.address, bookmark);
                    } else {
                        mBookmarks.delete(bookmark.address);
                        showBookmarks();
                    }
                })
                .show();
    }

    // "hifi://test.example.com:40102/0,0,0" is offered as "test.example.com"
    private static String suggestedName(String address) {
        String host = Uri.parse(address.contains("://") ? address : "hifi://" + address).getHost();
        return host != null && !host.isEmpty() ? host : address;
    }

    public interface OnHomeInteractionListener {
        void onSelectedDomain(String domainUrl);

        // A world from the list, with what the loading screen shows of it while it loads
        default void onSelectedWorld(String domainUrl, String name, String author, String picture) {
            onSelectedDomain(domainUrl);
        }
    }

    public void onSearchClear(View view) {
        mSearchView.setText("");
    }


}
