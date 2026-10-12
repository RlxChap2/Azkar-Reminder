const https = require('https');
const fs = require('fs');

function download(url, dest) {
    return new Promise((resolve, reject) => {
        const file = fs.createWriteStream(dest);
        https.get(url, (response) => {
            if (response.statusCode >= 300 && response.statusCode < 400 && response.headers.location) {
                return download(response.headers.location, dest).then(resolve).catch(reject);
            }
            response.pipe(file);
            file.on('finish', () => file.close(resolve));
        }).on('error', (err) => {
            fs.unlink(dest, () => reject(err));
        });
    });
}

async function run() {
    const vfUrl = 'http://vf.eg/vfcash?id=mt&qrId=Kke11v';
    // Pure clean IBAN so phone cameras treat it as plain text to Copy directly,
    // avoiding the Android "Bank:" protocol scheme collision bug!
    const ibanText = 'EG300002042004200383000005970';
    
    const vfApi = `https://api.qrserver.com/v1/create-qr-code/?size=400x400&margin=15&data=${encodeURIComponent(vfUrl)}`;
    const ibanApi = `https://api.qrserver.com/v1/create-qr-code/?size=400x400&margin=15&data=${encodeURIComponent(ibanText)}`;

    console.log('Downloading Vodafone QR...');
    await download(vfApi, 'assets/images/qr_vodafone.png');
    console.log('Downloading Pure IBAN QR...');
    await download(ibanApi, 'assets/images/qr_iban.png');
    console.log('Done!');
}

run().catch(console.error);
