package io.highfidelity.hifiinterface.view;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.cardview.widget.CardView;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

import io.highfidelity.hifiinterface.R;
import io.highfidelity.hifiinterface.provider.Bookmarks;
import io.highfidelity.hifiinterface.theme.Theme;
import io.highfidelity.hifiinterface.theme.ThemeStyler;

// Places' Bookmarks: the saved worlds that match the search box
public class BookmarkAdapter extends RecyclerView.Adapter<BookmarkAdapter.ViewHolder> {

    public interface Listener {
        void onBookmarkClicked(Bookmarks.Bookmark bookmark);
        void onBookmarkLongPressed(Bookmarks.Bookmark bookmark);
    }

    private final Theme mTheme;
    private final Listener mListener;
    private List<Bookmarks.Bookmark> mShown = new ArrayList<>();

    public BookmarkAdapter(Theme theme, Listener listener) {
        mTheme = theme;
        mListener = listener;
    }

    public void show(List<Bookmarks.Bookmark> bookmarks, String filterText) {
        mShown = new ArrayList<>();
        for (Bookmarks.Bookmark bookmark : bookmarks) {
            if (bookmark.matches(filterText)) {
                mShown.add(bookmark);
            }
        }
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.bookmark_item, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        Bookmarks.Bookmark bookmark = mShown.get(position);
        holder.name.setText(bookmark.name);
        holder.address.setText(bookmark.address);
        holder.card.setOnClickListener(view -> mListener.onBookmarkClicked(bookmark));
        holder.card.setOnLongClickListener(view -> {
            mListener.onBookmarkLongPressed(bookmark);
            return true;
        });
    }

    @Override
    public int getItemCount() {
        return mShown.size();
    }

    class ViewHolder extends RecyclerView.ViewHolder {
        final CardView card;
        final TextView name;
        final TextView address;

        ViewHolder(View view) {
            super(view);
            card = view.findViewById(R.id.bookmarkCard);
            name = view.findViewById(R.id.bookmarkName);
            address = view.findViewById(R.id.bookmarkAddress);
            ImageView icon = view.findViewById(R.id.bookmarkIcon);
            float density = view.getResources().getDisplayMetrics().density;
            card.setRadius(Theme.CORNERS_ROUNDED.equals(mTheme.corners) ? mTheme.cornerSizeDp * density : 0);
            card.setCardBackgroundColor(mTheme.surface);
            ThemeStyler.styleBody(name, mTheme);
            name.setTextColor(mTheme.text);
            ThemeStyler.styleMono(address, mTheme);
            address.setTextColor(mTheme.textSecondary);
            icon.setColorFilter(mTheme.primary);
        }
    }
}
