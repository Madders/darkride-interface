import { CommonModule } from "@angular/common";
import { HttpClientModule } from "@angular/common/http";
import { NgModule } from "@angular/core";
import { MatIconRegistry } from "@angular/material/icon";
import { DomSanitizer } from "@angular/platform-browser";
import { RouterModule, Routes } from "@angular/router";
import { MaterialModule } from "../material.module";
import { ExploreComponent } from "./explore.component";
import { FriendsComponent } from "./friends/friends.component";
import { HeaderComponent } from "./header/header.component";
import { InstanceBarComponent } from "./instance-bar/instance-bar.component";
import { WorldCardComponent } from "./world-card/world-card.component";
import { WorldsComponent } from "./worlds/worlds.component";

const routes: Routes = [
	{
		path: "",
		component: ExploreComponent,
	},
];

// the white rabbit on the Hop button, leaping right (drawn for Darkride; shapes take the text colour)
const RABBIT_ICON = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
	<ellipse cx="13.6" cy="5.4" rx="1.35" ry="4" transform="rotate(-38 13.6 5.4)" />
	<ellipse cx="15.6" cy="4.9" rx="1.25" ry="3.7" transform="rotate(-22 15.6 4.9)" />
	<ellipse cx="17.4" cy="9.6" rx="3.1" ry="2.6" transform="rotate(-18 17.4 9.6)" />
	<ellipse cx="11" cy="13" rx="6.8" ry="3.9" transform="rotate(-14 11 13)" />
	<circle cx="3.9" cy="13.4" r="1.7" />
	<ellipse cx="6.3" cy="17.2" rx="4.2" ry="1.35" transform="rotate(22 6.3 17.2)" />
	<ellipse cx="18.6" cy="14.6" rx="2.9" ry="0.95" transform="rotate(38 18.6 14.6)" />
</svg>`;

@NgModule({
	declarations: [
		ExploreComponent,
		HeaderComponent,
		InstanceBarComponent,
		WorldsComponent,
		WorldCardComponent,
		FriendsComponent,
	],
	imports: [
		CommonModule,
		RouterModule.forChild(routes),
		MaterialModule,
		HttpClientModule,
	],
})
export class ExploreModule {
	constructor(iconRegistry: MatIconRegistry, sanitizer: DomSanitizer) {
		iconRegistry.addSvgIconLiteral(
			"dr-rabbit",
			sanitizer.bypassSecurityTrustHtml(RABBIT_ICON),
		);
	}
}
