package io.highfidelity.hifiinterface.provider;

import android.util.Log;

import java.util.ArrayList;
import java.util.List;

import io.highfidelity.hifiinterface.HifiUtils;
import io.highfidelity.hifiinterface.view.DomainAdapter;
import retrofit2.Call;
import retrofit2.Response;
import retrofit2.Retrofit;
import retrofit2.converter.gson.GsonConverterFactory;
import retrofit2.http.GET;
import retrofit2.http.Query;

/**
 * Created by cduarte on 4/17/18.
 */

public class UserStoryDomainProvider implements DomainProvider {

    // Darkride's metaverse lists its worlds (domains) at /api/domains, as the desktop Explore app reads them, in
    // place of High Fidelity's /api/v1/user_stories
    public static final String BASE_URL = HifiUtils.METAVERSE_BASE_URL + "/";

    private static final int WORLDS_PER_PAGE = 50;
    private static final int MAX_PAGES_TO_GET = 10;

    private String mProtocol;
    private Retrofit mRetrofit;
    private WorldsService mWorldsService;

    private boolean startedToGetFromAPI = false;
    private List<World> allWorlds; // All retrieved worlds from the API
    private List<DomainAdapter.Domain> suggestions; // Filtered worlds to show

    public UserStoryDomainProvider(String protocol) {
        mRetrofit = new Retrofit.Builder()
                .baseUrl(BASE_URL)
                .addConverterFactory(GsonConverterFactory.create())
                .build();
        mWorldsService = mRetrofit.create(WorldsService.class);
        mProtocol = protocol;
        allWorlds = new ArrayList<>();
        suggestions = new ArrayList<>();
    }

    @Override
    public synchronized void retrieve(String filterText, DomainCallback domainCallback, boolean forceRefresh) {
        if (!startedToGetFromAPI || forceRefresh) {
            startedToGetFromAPI = true;
            fillDestinations(filterText, domainCallback);
        } else {
            filterChoicesByText(filterText, domainCallback);
        }
    }

    private void fillDestinations(String filterText, DomainCallback domainCallback) {
        List<World> worlds = new ArrayList<>();
        getWorldsPage(1, worlds, e -> {
            if (e != null) {
                startedToGetFromAPI = false;
                if (domainCallback != null) {
                    domainCallback.retrieveError(e, e.getMessage());
                }
                return;
            }
            allWorlds.clear();
            allWorlds.addAll(worlds);
            filterChoicesByText(filterText, domainCallback);
        });
    }

    private void getWorldsPage(int pageNumber, List<World> worlds, Callback<Exception> restOfPagesCallback) {
        Call<List<World>> call = mWorldsService.getDomains(pageNumber, WORLDS_PER_PAGE, "", mProtocol);
        Log.d("API-WORLDS", "Protocol [" + mProtocol + "] page [" + pageNumber + "]");
        call.enqueue(new retrofit2.Callback<List<World>>() {
            @Override
            public void onResponse(Call<List<World>> call, Response<List<World>> response) {
                List<World> page = response.body();
                if (!response.isSuccessful() || page == null) {
                    restOfPagesCallback.callback(new Exception("Error accessing url [" + call.request().url()
                            + "]: HTTP " + response.code()));
                    return;
                }
                worlds.addAll(page);
                if (page.size() >= WORLDS_PER_PAGE && pageNumber < MAX_PAGES_TO_GET) {
                    getWorldsPage(pageNumber + 1, worlds, restOfPagesCallback);
                    return;
                }
                restOfPagesCallback.callback(null);
            }

            @Override
            public void onFailure(Call<List<World>> call, Throwable t) {
                restOfPagesCallback.callback(new Exception("Error accessing url [" + call.request().url() + "]", t));
            }
        });
    }

    private boolean matches(World world, String[] words) {
        for (String word : words) {
            if (!world.searchText().contains(word)) {
                return false;
            }
        }
        return true;
    }

    private void filterChoicesByText(String filterText, DomainCallback domainCallback) {
        suggestions.clear();
        String trimmed = filterText == null ? "" : filterText.trim().toUpperCase();
        String[] words = trimmed.isEmpty() ? new String[]{} : trimmed.split("\\s+");
        for (World world : allWorlds) {
            if (world.online && matches(world, words)) {
                suggestions.add(world.toDomain());
            }
        }
        if (domainCallback != null) {
            domainCallback.retrieveOk(suggestions);
        }
    }

    public interface WorldsService {
        @GET("api/domains")
        Call<List<World>> getDomains(@Query("page") int pageNumber,
                                     @Query("amount") int amount,
                                     @Query("search") String search,
                                     @Query("protocol") String protocol);
    }

    class World {
        public World() {}
        String id;
        String label;
        String author;
        String description;
        String path;
        boolean online;
        int numUsers;
        private String searchText;

        String searchText() {
            if (searchText == null) {
                searchText = ((label == null ? "" : label) + " " + (author == null ? "" : author)).toUpperCase();
            }
            return searchText;
        }

        DomainAdapter.Domain toDomain() {
            // the address manager looks a world up by its ID, as the desktop Explore app joins it
            return new DomainAdapter.Domain(
                    label,
                    id + (path == null ? "" : path),
                    BASE_URL + "api/domain/" + id + "/image"
            );
        }
    }

}
