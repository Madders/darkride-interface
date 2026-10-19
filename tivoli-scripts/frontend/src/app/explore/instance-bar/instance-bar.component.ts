import { Component, OnDestroy, OnInit } from "@angular/core";
import { MatSnackBar } from "@angular/material/snack-bar";
import { Subscription } from "rxjs";
import { ScriptService } from "../../script.service";
import {
	CurrentInstance,
	ExploreService,
	InstanceVisibility,
} from "../explore.service";

const NULL_ID = "00000000-0000-0000-0000-000000000000";

// The instance (or instance world) the client is in: its link, and for the
// world's owner, edit mode and saving.
@Component({
	selector: "app-instance-bar",
	templateUrl: "./instance-bar.component.html",
	styleUrls: ["./instance-bar.component.scss"],
})
export class InstanceBarComponent implements OnInit, OnDestroy {
	current: CurrentInstance = null;
	domainId = "";
	busy = false;

	readonly visibilityIcons = {
		public: "public",
		friends: "group",
		private: "lock",
	};

	private subs: Subscription[] = [];

	constructor(
		private readonly scriptService: ScriptService,
		private readonly exploreService: ExploreService,
		private readonly snackBar: MatSnackBar,
	) {}

	ngOnInit() {
		this.subs.push(
			this.scriptService.event$.subscribe(data => {
				if (data.key == "getCurrentDomainId") {
					this.domainId = data.value ?? "";
					this.refresh();
				}
			}),
		);
		this.scriptService.emitEvent("explore", "getCurrentDomainId");
	}

	ngOnDestroy() {
		for (const sub of this.subs) sub.unsubscribe();
	}

	refresh() {
		if (!this.domainId || this.domainId == NULL_ID) {
			this.current = null;
			return;
		}
		this.exploreService.getCurrent(this.domainId).subscribe(
			current => (this.current = current),
			() => (this.current = null),
		);
	}

	private fail(what: string) {
		return err => {
			this.busy = false;
			this.snackBar.open(
				what +
					": " +
					(err?.error?.message ?? err?.message ?? "unknown error"),
				"OK",
				{ duration: 8000 },
			);
		};
	}

	get people() {
		const n = this.current?.instance?.players ?? 0;
		return n + (n == 1 ? " person" : " people");
	}

	onCopyLink() {
		this.scriptService
			.rpc("Window.copyToClipboard", [this.current.instance.address])
			.subscribe();
		this.snackBar.open("Copied " + this.current.instance.address, "OK", {
			duration: 4000,
		});
	}

	onEditMode(on: boolean) {
		this.busy = true;
		this.exploreService
			.setEditMode(this.current.instance.id, on)
			.subscribe(current => {
				this.busy = false;
				this.current = current;
				this.snackBar.open(
					on
						? `Edit mode is on in ${current.world.label}: you can build here. Save to update the world.`
						: "Edit mode is off",
					"OK",
					{ duration: 6000 },
				);
			}, this.fail("Could not change edit mode"));
	}

	onSave() {
		this.busy = true;
		this.snackBar.open("Saving the world...", null, { duration: 30000 });
		this.exploreService
			.saveInstance(this.current.instance.id)
			.subscribe(result => {
				this.busy = false;
				this.snackBar.open(
					`Saved ${this.current.world.label} as version ${result.version} (${result.entities} entities). New instances start from it.`,
					"OK",
					{ duration: 8000 },
				);
				this.refresh();
			}, this.fail("Could not save"));
	}

	onClose() {
		this.busy = true;
		this.exploreService
			.stopInstance(this.current.instance.id)
			.subscribe(() => {
				this.busy = false;
				this.current = null;
			}, this.fail("Could not close the instance"));
	}

	onNewInstance(visibility: InstanceVisibility) {
		this.busy = true;
		this.exploreService.setHopping(this.current.world.id);
		this.exploreService.hopStart(
			this.current.world.label,
			this.current.world.username,
			this.current.world.id,
		);
		this.exploreService
			.startInstance(this.current.world.id, visibility)
			.subscribe(
				instance => {
					this.busy = false;
					this.exploreService.joinInstance(instance);
				},
				err => {
					this.exploreService.setHopping(null);
					this.exploreService.hopFailed();
					this.fail("Could not start an instance")(err);
				},
			);
	}
}
