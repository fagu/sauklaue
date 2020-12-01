@0x9f493d0a1fbf378b;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("file4");

struct Point {
	x @0 :Int32;
	y @1 :Int32;
}

struct Path {
	points @0 :List(Point);
}

struct PenStroke {
	path @0 :Path;
	width @1 :Int32;
	color @2 :Int32;
}

struct EraserStroke {
	path @0 :Path;
	width @1 :Int32;
}

struct Stroke {
	union {
		pen @0 :PenStroke;
		eraser @1 :EraserStroke;
	}
}

struct NormalLayer {
	strokes @0 :List(Stroke);
}

struct EmbeddedPDF {
	name @0 :Text;
	contents @1 :Data;
}

struct PDFLayer {
	index @0 :Int32;
	page @1 :Int32;
	minPage @2 :Int32;
	maxPage @3 :Int32;
}

struct Layer {
	union {
		normal @0 :NormalLayer;
		pdf @1 :PDFLayer;
	}
}

struct Page {
	width @0 :Int32;
	height @1 :Int32;
	layers @2 :List(Layer);
}

struct File {
	pages @0 :List(Page);
	embeddedPDFs @1 :List(EmbeddedPDF);
}
