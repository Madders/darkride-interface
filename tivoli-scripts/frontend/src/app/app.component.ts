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

		// the overview floats over the world; every other panel fills its window (Tivoli's inline chat is gone: the
		// world's chat is the integrated chat's This world, Caity's bug batch 10-29)
		this.router.events
			.pipe(filter(e => e instanceof NavigationEnd))
			.subscribe((e: NavigationEnd) => {
				const overlay = /^\/overview\b/.test(e.urlAfterRedirects);
				document.documentElement.classList.toggle("dr-page", !overlay);
			});
	}
}
