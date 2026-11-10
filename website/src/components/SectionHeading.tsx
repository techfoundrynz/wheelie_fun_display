import { headingAnchorIcon } from "../heading-anchor";

export function SectionHeading({ id, children }: { id: string; children: string }) {
  return (
    <h2 id={id}>
      {children}
      <a
        className="heading-anchor"
        href={"#" + id}
        aria-label={"Link to " + children}
        title={"Link to " + children}
        dangerouslySetInnerHTML={{ __html: headingAnchorIcon }}
      />
    </h2>
  );
}
