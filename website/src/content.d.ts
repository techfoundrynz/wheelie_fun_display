declare module "virtual:docs" {
  export interface PageMeta {
    source: string;
    route: string;
    title: string;
    group: string;
    description: string;
    search: string;
  }
  export interface DocPage extends PageMeta {
    body: string;
    headings: { id: string; title: string; level: number }[];
  }
  export const pages: PageMeta[];
  export function loadDoc(route: string): Promise<DocPage | null>;
}
