package io.highfidelity.hifiinterface.view;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.net.Uri;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import com.squareup.picasso.Picasso;

import java.util.ArrayList;
import java.util.List;

import io.highfidelity.hifiinterface.R;
import io.highfidelity.hifiinterface.provider.AvatarProvider;
import io.highfidelity.hifiinterface.theme.ThemeManager;

/**
 * Created by gcalero on 1/21/19
 */
public class AvatarAdapter extends RecyclerView.Adapter<AvatarAdapter.ViewHolder> {

    private static final String TAG = "Interface";
    private static final float PLACEHOLDER_INSET = 0.2f;
    private final Context mContext;
    private final LayoutInflater mInflater;
    private final AvatarProvider mProvider;
    private List<Avatar> mAvatars = new ArrayList<>();
    private ItemClickListener mClickListener;

    public AvatarAdapter(Context context, AvatarProvider provider) {
        mContext = context;
        mInflater = LayoutInflater.from(mContext);
        mProvider = provider;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view = mInflater.inflate(R.layout.avatar_item, parent, false);
        return new AvatarAdapter.ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        AvatarAdapter.Avatar anAvatar = mAvatars.get(position);
        assert(holder.mName != null);
        holder.mName.setText(anAvatar.avatarName);
        // avatars without a picture show a figure, in the theme
        if (anAvatar.avatarPreviewUrl == null || anAvatar.avatarPreviewUrl.isEmpty()) {
            Picasso.get().cancelRequest(holder.mPreviewImage);
            holder.mPreviewImage.setImageDrawable(placeholder());
        } else {
            Picasso.get().load(Uri.parse(anAvatar.avatarPreviewUrl))
                    .placeholder(placeholder())
                    .error(placeholder())
                    .into(holder.mPreviewImage);
        }
    }

    // A figure in the theme's secondary colour, with room round it
    private Drawable placeholder() {
        Drawable figure = ContextCompat.getDrawable(mContext, R.drawable.ic_avatar_placeholder).mutate();
        figure.setTint(ThemeManager.get(mContext).textSecondary);
        return new InsetDrawable(figure, PLACEHOLDER_INSET);
    }

    @Override
    public int getItemCount() {
        return mAvatars.size();
    }

    public void loadAvatars() {
        mProvider.retrieve(new AvatarProvider.AvatarsCallback() {
            @Override
            public void retrieveOk(List<AvatarAdapter.Avatar> avatars) {
                mAvatars = new ArrayList<>(avatars);
                notifyDataSetChanged();
            }

            @Override
            public void retrieveError(Exception e, String message) {
                Log.e(TAG, message, e);
            }
        });
    }

    public void setClickListener(ItemClickListener clickListener) {
        mClickListener = clickListener;
    }

    public interface ItemClickListener {
        void onItemClick(View view, int position, Avatar avatar);
    }

    public class ViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {

        TextView mName;
        ImageView mPreviewImage;

        public ViewHolder(View itemView) {
            super(itemView);
            mName = itemView.findViewById(R.id.avatarName);
            assert (mName != null);
            mPreviewImage = itemView.findViewById(R.id.avatarPreview);
            itemView.setOnClickListener(this);
        }

        @Override
        public void onClick(View view) {
            int position= getAdapterPosition();
            if (mClickListener != null) {
                mClickListener.onItemClick(view, position, mAvatars.get(position));
            }
        }
    }

    public static class Avatar {
        public String avatarName;
        public String avatarUrl;
        public String avatarPreviewUrl;

        public Avatar() { }
    }
}
