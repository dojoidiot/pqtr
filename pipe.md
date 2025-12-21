The pipe.json is a repeatable program that is extended by the labs app.  The pipe.json will also be able to be run by a headless labs app.

The rules are strict.  Everything is a Link in the pipe.  No code outside of Links (fix encodePng and decodeJpeg)

The pipe has two execution flows - the "tune" flow which tunes the pipe info for use by the "vibe" flow.  

The first phase of tune does the lute and drum process on the on camera jpeg.  
The second phase of tune does a default vibe optimisation to get the 0 vibe settings.
This creates the default pipe.

A user may add another image to the project.  It is put into a sub folder - 
lute may update its gear data as shown below.

There is always a tune flow for the on camera JPEG.  It runs on info that is built by tune which puts its data into the info area for the steps used in vibes. 


The user LABS area is organised as:
LABS/<user itag>/
    gear/
    pipe/

The gear area is where lute makes its tune data for specific gear and styles:

LABS/<user itag>/gear/<gear name>/<style name>.lute.json

The vibe section is an array because vibes will be added to compare to other photos - not just the RAW jpeg which forms the baseline 0 vibe.  These files will be added to the pipe folder with the basename and their index and type.

The pipe data:

{
    pipe: "version",
    head:{
        camera loaded info
    },
    body: {
        lute:{},
        drum:{},
        vibe: [ 
            {}, // default vibe from on board JPG tune.
            // additional vibe settings.
        ]
    },
    tail: "the pipe output type that is posted - png is the default.  Can add zip later"
    vibe-list: [  
        // default vibe 
        [
            file: "DSC00144.ARW"
            name: "enter this on load - allow edit",
            find: "search terms on load - allow edit"
        ],
        // additional vibe file settings
    ]
}

the pipe area is organised sidecar style:

<raw basename>.ARW (or other type)
<basename>.0.jpg (the on camera jpeg loaded in the head as the )
<basename>.png (the tail file - always made)
<basename>.pipe.json (the pipe file)
<basename>.head.png (the linear raw made at the head of the pipe)
<basename>.lute.png (output of lute)
<basename>.drum.png (output of drum)
<basename>.vibe.0.png (output of the tune vibe)
<basename>.diff.png (the final diff image of <basename>.jpg and <basename>.png)
<basename>.zip (the tail file if zip mode)

