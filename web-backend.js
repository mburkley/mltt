/*  Nodejs backend wrapper for command line tools mltt-tape and mltt-file.  Uploads a .wav file
 *  and presents options for user to decode as basic program or download a new re-generated
 *  .wav file */

var http = require ('http');
var url = require('url');
var fs = require('fs');
var qs = require('querystring');
var formidable = require('formidable');
var spawn = require('child_process').spawn;

const hostname = 'localhost';
const port = 8080;

/*  Present an inital file upload form with action of menu */
function formUpload (req, res)
{
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.write('<h1>TI99/4A cassette WAV file upload</h1>');
    res.write('<form action="/script/menu" method="post" enctype="multipart/form-data">');
    res.write('<input type="file" name="filetoupload"><br>');
    res.write('<input type="submit">');
    res.write('</form>');
    return res.end();
}

/*  Spawn an external process and its output to the res stream.  Call the done callback when
 *  the process completes */
function runProcess (res, program, options, done)
{
    var proc = spawn(program, options);

    proc.stdout.on('data', function (data) {
        res.write (data.toString());
    });

    proc.stderr.on('data', function (data) {
        res.write (data.toString());
    });

    proc.on('close', function (code) {
        console.log('process exit code ' + code);
        done();
    });
}

function button (res, name, value, text)
{
    res.write('<p><button name="'+name+'" value="'+value+'" type="submit">'+text+'</button>');
}

/*  Close the menu form and add current parameters for post */
function menuReturnWithParams (res, origName, uploadName, binName)
{
    res.write('<input type="hidden" value="'+origName+'" name="origName">');
    res.write('<input type="hidden" value="'+uploadName+'" name="uploadName">');
    res.write('<input type="hidden" value="'+binName+'" name="binName">');
    res.write('</form>');
    res.write ('</body></html>');
    res.end();
}

/*  Create a button to allow return to the menu after an operation */
function menuReturn (res, origName, uploadName, binName)
{
    console.log ('menu return');
    res.write ('</pre>');
    button (res, 'menu', 'continue', 'Continue');
    menuReturnWithParams (res, origName, uploadName, binName);
}

function formAction (req, res)
{
    if (req.method != 'POST')
    {
        res.writeHead (400, {'Content-Type': 'text/html'});
        res.end('expected post');
        return;
    }

    var body = '';

    req.on('data', (data) => {
        body += data;
        console.log ("decode, data="+data);
    });

    req.on('end', () => {
        var post = qs.parse(body);
        console.log ('decode, qs-f='+JSON.stringify(post));
        let origName = post['origName'];
        let uploadName = post['uploadName'];
        let uploadFile = '/tmp/' + uploadName;
        let binName = uploadName + '.bin';
        let binFile = '/tmp/' + binName;
        let downloadName = 'NEW-'+origName;
        let downloadFile = '../download/'+downloadName;

        res.writeHead (200, {'Content-Type': 'text/html'});
        res.write ('<html>');
        res.write ('<head><title>Decode</title></head>');
        res.write ('<body>');
        res.write ('<pre>');

        res.write('<form action="/script/menu" method="post">');

        if (post.action == 'verbose')
        {
            let options = ['-v', '-e', binFile, uploadFile];
            runProcess (res, './mltt-tape', options, function done () {
                menuReturn (res, origName, uploadName, binName);
            });
        }

        if (post.action == 'decode')
        {
            let options = ['-e', binFile, uploadFile];
            runProcess (res, './mltt-tape', options, function done () {
                menuReturn (res, origName, uploadName, binName);
            });
        }

        if (post.action == 'basic')
        {
            let options = [ '-x', '-b', binFile ];
            runProcess (res, "./mltt-file", options, function done () {
                menuReturn (res, origName, uploadName, binName);
            });
        }

        if (post.action == 'create')
        {
            let options = [ '-c', binFile, downloadFile ];
            runProcess (res, "./mltt-tape", options, function done () {
                res.write ('<br>Download new WAV file <a href='+downloadFile+'>here</a>');
                menuReturn (res, origName, uploadName, binName);
            });
        }
    });
}

function formMenu (req, res)
{
    if (req.method != 'POST')
    {
        res.writeHead (400, {'Content-Type': 'text/html'});
        res.end('expected post');
        return;
    }

    var form = new formidable.IncomingForm();
    form.parse(req, function (err, fields, files)
    {
        res.writeHead(200, {'Content-Type': 'text/html'});
        res.write('<h1>Main menu</h1>');

        var origName;
        var uploadName;
        var binName;

        if (fields)
        {
            console.log ('menu fields='+JSON.stringify(fields));
            origName = fields['origName'];
            uploadName = fields['uploadName'];
            binName = fields['binName'];
        }

        if (files && files.filetoupload)
        {
            console.log("original file="+files.filetoupload[0].originalFilename);
            console.log("upload file="+files.filetoupload[0].newFilename);
            origName = files.filetoupload[0].originalFilename;
            uploadName = files.filetoupload[0].newFilename;
        }
        console.log("====================");

        res.write ('<html>'+ '<head><title>Uploaded</title></head>'+ '<body>');

        if (binName)
            res.write('File '+origName+' has been uploaded and decoded.');
        else
            res.write('File '+origName+' has been uploaded.');

        res.write('What would you like to do next?');
        res.write('<form action="/script/action" method="post">');

        /* If we have a binary file name, then file has already been decodede, so present
         * procesing options */
        if (binName)
        {
            button (res, 'action', 'basic', 'Show basic code');
            button (res, 'action', 'create', 'Create new WAV file');
        }
        else
        {
            button (res, 'action', 'decode', 'Decode file');
            button (res, 'action', 'verbose', 'Decode with hex dump');
        }
        menuReturnWithParams (res, origName, uploadName, binName)
    });
}

const server=http.createServer((request, response) => {
    var q = url.parse(request.url, true);
    var s = q.pathname.split('/');

    switch (s[1])
    {
    case "menu":
        formMenu (request, response);
        break;
    case "action":
        formAction (request, response);
        break;
    case "upload":
    default:
        formUpload (request, response);
        break;
    }
});

server.listen(port,hostname, () => {
    console.log('launch server running at http://'+hostname+':'+port);
});;


