import { HttpClient, HttpParams } from "@angular/common/http";
import { Injectable } from "@angular/core";
import { forkJoin, Observable, of, Subject } from "rxjs";
import { catchError } from "rxjs/operators";
import { ScriptService } from "../script.service";

export interface World {
	id: string;
	label: string;
	username: string;
	description: string;
	restriction: "open" | "hifi" | "acl";

	online: boolean;
	numUsers: number;
	likes: number;
	liked: boolean;

	networkAddress: string;
	networkPort: string;

	protocol: string;
	version: string;

	path: string;
	url: string;

	// set when the world can be opened as disposable instances
	instances?: WorldInstances;
}

export type InstanceVisibility = "public" | "friends" | "private";

export interface WorldInstances {
	access: "public" | "friends" | "private";
	instanceOnly: boolean;
	live: boolean; // the source of truth is an always-on domain
	version: number;
	savedAt: string;
	canStart: boolean;
	canEdit: boolean;
	list: Instance[];
}

export interface Instance {
	id: string;
	world: string;
	label: string;
	visibility: InstanceVisibility;
	players: number;
	address: string;
	startedBy: string;
	createdAt: string;
	version: number;
	latest: boolean;
	editing: boolean;
	editors: string[];
	canStop?: boolean;
}

// the domain the client is in, when it is an instance or a world's own domain
export interface CurrentInstance {
	world: World;
	instance: Instance | null;
	editingBy?: string;
}

export interface Friend {
	username: string;
	online: boolean;
	trusted: boolean;
	image: string;
	domain: {
		id: string;
		name: string;
	};
}

@Injectable({
	providedIn: "root",
})
export class ExploreService {
	loading = false;

	// the world whose instance chooser is open, so the other worlds can step back
	chooserWorldId: string = null;

	worlds: World[] = [];
	type: "popular" | "liked" | "private" = "popular";
	search = "";
	page = 1;
	protocol = "";

	friends$ = new Subject<Friend[]>();

	constructor(
		private scriptService: ScriptService,
		private http: HttpClient,
	) {
		if (this.scriptService.hasQt) {
			this.scriptService
				.rpc<string>("Window.protocolSignature()")
				.subscribe(protocol => {
					this.protocol = protocol;
					this.loadWorlds();
				});
		} else {
			this.loadWorlds();
		}

		this.loadFriends();
	}

	loadWorlds(reset = true) {
		this.loading = true;
		this.page = reset ? 1 : this.page + 1;

		// worlds that can be opened as instances are listed with the online domains,
		// whether or not anyone is in them
		const instanceWorlds: Observable<World[]> =
			this.type == "popular" && this.page == 1
				? this.http
						.get<World[]>(
							this.scriptService.metaverseUrl + "/api/worlds",
							{
								params: new HttpParams().set(
									"search",
									this.search,
								),
							},
						)
						.pipe(catchError(() => of([] as World[])))
				: of([] as World[]);

		const domains = this.http
			.get<World[]>(
				this.scriptService.metaverseUrl +
					(() => {
						const t = this.type;
						if (t == "popular") return "/api/domains";
						if (t == "liked") return "/api/user/domains/liked";
						if (t == "private") return "/api/user/domains/private";
					})(),
				{
					params: new HttpParams()
						.set("page", this.page + "")
						.set("amount", 50 + "")
						.set("search", this.search)
						.set("protocol", this.protocol),
				},
			)
			.pipe(catchError(() => of([] as World[])));

		forkJoin([domains, instanceWorlds]).subscribe(
			([online, withInstances]) => {
				const worlds = this.mergeWorlds(online, withInstances);
				this.worlds = reset ? worlds : [...this.worlds, ...worlds];
				this.loading = false;
			},
			err => {
				this.worlds = reset ? [] : this.worlds;
				this.loading = false;
			},
		);
	}

	private mergeWorlds(online: World[], withInstances: World[]) {
		const byId = new Map(withInstances.map(w => [w.id, w]));
		const merged = online.map(w => byId.get(w.id) ?? w);
		const listed = new Set(merged.map(w => w.id));
		const people = (w: World) =>
			w.numUsers +
			(w.instances?.list ?? []).reduce((n, i) => n + i.players, 0);
		const rest = withInstances
			.filter(w => !listed.has(w.id))
			.sort((a, b) => people(b) - people(a));
		// worlds with people in them first, then the rest of the instance worlds
		return [
			...merged,
			...rest.filter(w => people(w) > 0),
			...rest.filter(w => people(w) == 0),
		];
	}

	// instances

	startInstance(worldId: string, visibility: InstanceVisibility) {
		return this.http.post<Instance>(
			this.scriptService.metaverseUrl +
				"/api/worlds/" +
				worldId +
				"/instances",
			{ visibility },
		);
	}

	joinInstance(instance: Instance) {
		this.scriptService.emitEvent("explore", "joinDomain", instance.address);
		this.scriptService.emitEvent("explore", "close");
	}

	getCurrent(domainId: string) {
		return this.http.get<CurrentInstance>(
			this.scriptService.metaverseUrl +
				"/api/instances/by-domain/" +
				domainId,
		);
	}

	setEditMode(instanceId: string, on: boolean) {
		return this.http.post<CurrentInstance>(
			this.scriptService.metaverseUrl +
				"/api/instances/" +
				instanceId +
				"/edit",
			{ on },
		);
	}

	saveInstance(instanceId: string) {
		return this.http.post<{ version: number; entities: number }>(
			this.scriptService.metaverseUrl +
				"/api/instances/" +
				instanceId +
				"/save",
			null,
		);
	}

	stopInstance(instanceId: string) {
		return this.http.delete(
			this.scriptService.metaverseUrl + "/api/instances/" + instanceId,
		);
	}

	likeWorld(id: string, like = true) {
		return this.http.post(
			this.scriptService.metaverseUrl +
				"/api/domains/" +
				id +
				"/" +
				(like ? "like" : "unlike"),
			null,
		);
	}

	loadFriends() {
		return this.http
			.get<Friend[]>(
				this.scriptService.metaverseUrl + "/api/user/friends",
			)
			.subscribe(
				friends => {
					this.friends$.next(friends);
				},
				err => {
					this.friends$.next([]);
				},
			);
	}
}
