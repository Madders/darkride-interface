import { Component, OnInit } from "@angular/core";
import { ActivatedRoute, NavigationEnd, Router } from "@angular/router";
import { filter } from "rxjs/operators";
import { ScriptService } from "./script.service";

@Component({
	selector: "app-root",
	templateUrl: "app.component.html",
})
export class AppComponent implements OnInit {
	constructor(
		private scriptService: ScriptService,
		private route: ActivatedRoute,
		private router: Router,
	) {}

	ngOnInit() {
		this.route.queryParams.subscribe(params => {
			this.scriptService.metaverseUrl = params.metaverseUrl;
		});

		// chat and the overview float over the world; every other panel fills its window
		this.router.events
			.pipe(filter(e => e instanceof NavigationEnd))
			.subscribe((e: NavigationEnd) => {
				const overlay = /^\/(chat|overview)\b/.test(e.urlAfterRedirects);
				document.documentElement.classList.toggle("dr-page", !overlay);
			});
	}
}
