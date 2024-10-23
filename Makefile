.POSIX:

VERSION=0.1

INKSCAPE=inkscape

OBJECTS=\
	approachcircle.png \
	cursor.png \
	cursormiddle.png \
	cursortrail.png \
	hit0.png \
	hit0-0.png \
	hit0-1.png \
	hit0-2.png \
	hit0-3.png \
	hit0-4.png \
	hit0-5.png \
	hit0-6.png \
	hit0-7.png \
	hit0-8.png \
	hit0-9.png \
	hit0-10.png \
	hit0-11.png \
	hit0-12.png \
	hit0-13.png \
	hit0-14.png \
	hit50.png \
	hit50-0.png \
	hit50-1.png \
	hit50-2.png \
	hit50-3.png \
	hit50-4.png \
	hit50-5.png \
	hit50-6.png \
	hit50-7.png \
	hit50-8.png \
	hit50-9.png \
	hit50-10.png \
	hit50-11.png \
	hit50-12.png \
	hit50-13.png \
	hit50-14.png \
	hit100.png \
	hit100-0.png \
	hit100-1.png \
	hit100-2.png \
	hit100-3.png \
	hit100-4.png \
	hit100-5.png \
	hit100-6.png \
	hit100-7.png \
	hit100-8.png \
	hit100-9.png \
	hit100-10.png \
	hit100-11.png \
	hit100-12.png \
	hit100-13.png \
	hit100-14.png \
	hit100k.png \
	hit100k-0.png \
	hit100k-1.png \
	hit100k-2.png \
	hit100k-3.png \
	hit100k-4.png \
	hit100k-5.png \
	hit100k-6.png \
	hit100k-7.png \
	hit100k-8.png \
	hit100k-9.png \
	hit100k-10.png \
	hit100k-11.png \
	hit100k-12.png \
	hit100k-13.png \
	hit100k-14.png \
	hit300.png \
	hit300-0.png \
	hit300k.png \
	hit300k-0.png \
	hit300g.png \
	hit300g-0.png \
	hitcircle.png \
	hitcircleoverlay.png \
	hitcircleselect.png \
	reversearrow.png \
	scorebar-bg.png \
	scorebar-colour.png \
	scorebar-ki.png \
	scorebar-kidanger.png \
	scorebar-kidanger2.png \
	score-0.png \
	score-1.png \
	score-2.png \
	score-3.png \
	score-4.png \
	score-5.png \
	score-6.png \
	score-7.png \
	score-8.png \
	score-9.png \
	score-comma.png \
	score-dot.png \
	score-percent.png \
	score-x.png \
	sliderb.png \
	sliderendcircle.png \
	sliderendcircleoverlay.png \
	sliderendmiss-0.png \
	sliderendmiss-1.png \
	sliderendmiss-2.png \
	sliderendmiss-3.png \
	sliderendmiss-4.png \
	sliderendmiss-5.png \
	sliderendmiss-6.png \
	sliderendmiss-7.png \
	sliderendmiss-8.png \
	sliderendmiss-9.png \
	sliderendmiss-10.png \
	sliderendmiss-11.png \
	sliderendmiss-12.png \
	sliderendmiss-13.png \
	sliderendmiss-14.png \
	sliderfollowcircle.png \
	sliderscorepoint.png \
	slidertickmiss-0.png \
	slidertickmiss-1.png \
	slidertickmiss-2.png \
	slidertickmiss-3.png \
	slidertickmiss-4.png \
	slidertickmiss-5.png \
	slidertickmiss-6.png \
	slidertickmiss-7.png \
	slidertickmiss-8.png \
	slidertickmiss-9.png \
	slidertickmiss-10.png \
	slidertickmiss-11.png \
	slidertickmiss-12.png \
	slidertickmiss-13.png \
	slidertickmiss-14.png

RESOURCES=\
	skin.ini

.PHONY: all
all: irodori-$(VERSION).osk

.SUFFIXES: .svg .png 
.svg.png:
	$(INKSCAPE) --export-type=png --export-filename=$@ $<
	$(INKSCAPE) --export-type=png \
		--export-dpi=192 --export-filename=$*@2x.png $<

irodori-$(VERSION).osk: $(OBJECTS) $(RESOURCES)
	zip -r $@ \
		$(OBJECTS) \
		$(OBJECTS:.png=@2x.png)\
		$(RESOURCES)

hit0-0.svg: animations/animate_miss.py animations/hit0.svg
	animations/animate_miss.py animations/hit0.svg .
hit0-1.svg: hit0-0.svg
hit0-2.svg: hit0-0.svg
hit0-3.svg: hit0-0.svg
hit0-4.svg: hit0-0.svg
hit0-5.svg: hit0-0.svg
hit0-6.svg: hit0-0.svg
hit0-7.svg: hit0-0.svg
hit0-8.svg: hit0-0.svg
hit0-9.svg: hit0-0.svg
hit0-10.svg: hit0-0.svg
hit0-11.svg: hit0-0.svg
hit0-12.svg: hit0-0.svg
hit0-13.svg: hit0-0.svg
hit0-14.svg: hit0-0.svg

hit50-0.svg: animations/animate_hit.py animations/hit50.svg
	animations/animate_hit.py animations/hit50.svg .
hit50-1.svg: hit50-0.svg
hit50-2.svg: hit50-0.svg
hit50-3.svg: hit50-0.svg
hit50-4.svg: hit50-0.svg
hit50-5.svg: hit50-0.svg
hit50-6.svg: hit50-0.svg
hit50-7.svg: hit50-0.svg
hit50-8.svg: hit50-0.svg
hit50-9.svg: hit50-0.svg
hit50-10.svg: hit50-0.svg
hit50-11.svg: hit50-0.svg
hit50-12.svg: hit50-0.svg
hit50-13.svg: hit50-0.svg
hit50-14.svg: hit50-0.svg

hit100-0.svg: animations/animate_hit.py animations/hit100.svg
	animations/animate_hit.py animations/hit100.svg .
hit100-1.svg: hit100-0.svg
hit100-2.svg: hit100-0.svg
hit100-3.svg: hit100-0.svg
hit100-4.svg: hit100-0.svg
hit100-5.svg: hit100-0.svg
hit100-6.svg: hit100-0.svg
hit100-7.svg: hit100-0.svg
hit100-8.svg: hit100-0.svg
hit100-9.svg: hit100-0.svg
hit100-10.svg: hit100-0.svg
hit100-11.svg: hit100-0.svg
hit100-12.svg: hit100-0.svg
hit100-13.svg: hit100-0.svg
hit100-14.svg: hit100-0.svg

hit100k-0.svg: animations/animate_hit.py animations/hit100k.svg
	animations/animate_hit.py animations/hit100k.svg .
hit100k-1.svg: hit100k-0.svg
hit100k-2.svg: hit100k-0.svg
hit100k-3.svg: hit100k-0.svg
hit100k-4.svg: hit100k-0.svg
hit100k-5.svg: hit100k-0.svg
hit100k-6.svg: hit100k-0.svg
hit100k-7.svg: hit100k-0.svg
hit100k-8.svg: hit100k-0.svg
hit100k-9.svg: hit100k-0.svg
hit100k-10.svg: hit100k-0.svg
hit100k-11.svg: hit100k-0.svg
hit100k-12.svg: hit100k-0.svg
hit100k-13.svg: hit100k-0.svg
hit100k-14.svg: hit100k-0.svg

sliderendmiss-0.svg: animations/animate_miss.py animations/sliderendmiss.svg
	animations/animate_miss.py animations/sliderendmiss.svg .
sliderendmiss-1.svg: sliderendmiss-0.svg
sliderendmiss-2.svg: sliderendmiss-0.svg
sliderendmiss-3.svg: sliderendmiss-0.svg
sliderendmiss-4.svg: sliderendmiss-0.svg
sliderendmiss-5.svg: sliderendmiss-0.svg
sliderendmiss-6.svg: sliderendmiss-0.svg
sliderendmiss-7.svg: sliderendmiss-0.svg
sliderendmiss-8.svg: sliderendmiss-0.svg
sliderendmiss-9.svg: sliderendmiss-0.svg
sliderendmiss-10.svg: sliderendmiss-0.svg
sliderendmiss-11.svg: sliderendmiss-0.svg
sliderendmiss-12.svg: sliderendmiss-0.svg
sliderendmiss-13.svg: sliderendmiss-0.svg
sliderendmiss-14.svg: sliderendmiss-0.svg

slidertickmiss-0.svg: animations/animate_miss.py animations/slidertickmiss.svg
	animations/animate_miss.py animations/slidertickmiss.svg .
slidertickmiss-1.svg: slidertickmiss-0.svg
slidertickmiss-2.svg: slidertickmiss-0.svg
slidertickmiss-3.svg: slidertickmiss-0.svg
slidertickmiss-4.svg: slidertickmiss-0.svg
slidertickmiss-5.svg: slidertickmiss-0.svg
slidertickmiss-6.svg: slidertickmiss-0.svg
slidertickmiss-7.svg: slidertickmiss-0.svg
slidertickmiss-8.svg: slidertickmiss-0.svg
slidertickmiss-9.svg: slidertickmiss-0.svg
slidertickmiss-10.svg: slidertickmiss-0.svg
slidertickmiss-11.svg: slidertickmiss-0.svg
slidertickmiss-12.svg: slidertickmiss-0.svg
slidertickmiss-13.svg: slidertickmiss-0.svg
slidertickmiss-14.svg: slidertickmiss-0.svg

.PHONY: clean
clean:
	-rm hit0-*.svg
	-rm hit50-*.svg
	-rm hit100-*.svg
	-rm hit100k-*.svg
	-rm sliderendmiss-*.svg
	-rm slidertickmiss-*.svg
	-rm $(OBJECTS)
	-rm *@2x.png
	-rm *.osk
