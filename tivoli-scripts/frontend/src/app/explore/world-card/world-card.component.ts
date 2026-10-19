import { Component, Input } from "@angular/core";
import { MatSnackBar } from "@angular/material/snack-bar";
import { ScriptService } from "../../script.service";
import {
	ExploreService,
	Instance,
	InstanceVisibility,
	World,
} from "../explore.service";

// somewhere in a world a person can be sent: the world's own always-on server, or one of its instances
export interface Place {
	name: string;
	detail: string;
	icon: string;
	people: number;
	instance?: Instance; // none for the world's own server
}

const visibilityIcons: { [key in InstanceVisibility]: string } = {
	public: "public",
	friends: "group",
	private: "lock",
};

const visibilityNames: { [key in InstanceVisibility]: string } = {
	public: "Public",
	friends: "Friends",
	private: "Private",
};

const plural = (n: number, singular: string, plural?: string) =>
	n + " " + (n == 1 ? singular : plural != null ? plural : singular + "s");

@Component({
	selector: "app-world-card",
	templateUrl: "./world-card.component.html",
	styleUrls: ["./world-card.component.scss"],
})
export class WorldCardComponent {
	@Input() world: World;

	starting = false;

	constructor(
		public readonly exploreService: ExploreService,
		public readonly scriptService: ScriptService,
		private readonly snackBar: MatSnackBar,
	) {}

	// an instance-only world's own domain, if it runs at all, is its source, not a place for visitors
	private get ownServerOpen() {
		return this.world.online && !this.world.instances?.instanceOnly;
	}

	// where people can go now, the busiest first (the world's own server first on a tie)
	get places(): Place[] {
		const places: Place[] = [];
		if (this.ownServerOpen) {
			places.push({
				name: "Always on",
				detail:
					"The world's own server · " +
					plural(this.world.numUsers, "person", "people"),
				icon: "dns",
				people: this.world.numUsers,
			});
		}
		for (const instance of this.world.instances?.list ?? []) {
			places.push({
				name: visibilityNames[instance.visibility] + " instance",
				detail: [
					plural(instance.players, "person", "people"),
					instance.startedBy
						? "started by " + instance.startedBy
						: null,
					instance.latest ? null : "an older version",
					instance.editing ? "being edited" : null,
				]
					.filter(text => text != null)
					.join(" · "),
				icon: visibilityIcons[instance.visibility],
				people: instance.players,
				instance,
			});
		}
		return places.sort((a, b) => b.people - a.people);
	}

	get people() {
		return this.places.reduce((n, place) => n + place.people, 0);
	}

	get canHop() {
		return this.places.length > 0 || !!this.world.instances?.canStart;
	}

	get incompatible() {
		return this.exploreService.protocol && this.world.protocol
			? this.world.protocol != this.exploreService.protocol
			: false;
	}

	get status() {
		const people = this.people;
		if (people > 0) {
			const busy = this.places.filter(place => place.people > 0).length;
			return (
				plural(people, "person", "people") +
				" here" +
				(busy > 1 ? " across " + busy + " instances" : "")
			);
		}
		if (this.places.length > 0) return "Open now. Be the first one in.";
		if (this.world.instances?.canStart)
			return "Hop opens a new instance for you.";
		return "Sign in to open this world.";
	}

	// Hop: where the people are; else somewhere already running; else a new public instance
	onHop() {
		if (this.starting || this.exploreService.hoppingWorldId != null) return;
		const places = this.places;
		const open =
			places.find(place => place.people > 0) ??
			places.find(place => place.instance == null) ??
			places.find(place => place.instance.visibility == "public") ??
			(this.world.instances?.canStart ? null : places[0]);
		if (open != null) {
			this.onJoin(open);
		} else if (this.world.instances?.canStart) {
			this.onNewInstance("public");
		}
	}

	onJoin(place: Place) {
		this.starting = true; // "Hopping..." until the Explore closes behind the client
		this.exploreService.hopStart(this.world.label, this.world.username, this.world.id);
		if (place.instance != null) {
			this.exploreService.joinInstance(place.instance);
		} else {
			this.onJoinWorld();
		}
	}

	onNewInstance(visibility: InstanceVisibility) {
		if (this.starting || this.exploreService.hoppingWorldId != null) return;
		this.starting = true;
		this.exploreService.setHopping(this.world.id);
		this.exploreService.hopStart(this.world.label, this.world.username, this.world.id);
		this.exploreService.startInstance(this.world.id, visibility).subscribe(
			instance => {
				this.exploreService.joinInstance(instance);
			},
			err => {
				this.starting = false;
				this.exploreService.setHopping(null);
				this.exploreService.hopFailed();
				this.snackBar.open(
					"Could not start an instance: " +
						(err?.error?.message ??
							err?.message ??
							"unknown error"),
					"OK",
					{ duration: 8000 },
				);
			},
		);
	}

	onJoinWorld() {
		this.exploreService.setHopping(this.world.id);
		this.scriptService.emitEvent(
			"explore",
			"joinDomain",
			this.world.id + this.world.path,
		);
		this.scriptService.emitEvent("explore", "close");
	}

	onLikeWorld() {
		this.exploreService
			.likeWorld(this.world.id, !this.world.liked)
			.subscribe(() => {
				this.world.liked = !this.world.liked;
				if (this.world.liked) {
					this.world.likes++;
				} else {
					this.world.likes--;
				}
			});
	}
}
